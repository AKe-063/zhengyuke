#include "stdafx.h"
#include "ForwardRenderer.h"
#include "Engine.h"

ForwardRenderer::ForwardRenderer()
	:rhi(nullptr)
{
	rhi->CreateRHI();
}

ForwardRenderer::~ForwardRenderer()
{

}

void ForwardRenderer::Init()
{
	fShaderManager = std::make_shared<FShaderManager>();
	std::vector<INPUT_ELEMENT_DESC> mInputLayout =
	{
		{ "POSITION", 0, INPUT_FORMAT::INPUT_FORMAT_R32G32B32_FLOAT, 0, 0, INPUT_CLASSIFICATION::INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TangentY", 0, INPUT_FORMAT::INPUT_FORMAT_R32G32B32A32_FLOAT, 0, 12, INPUT_CLASSIFICATION::INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TangentX", 0, INPUT_FORMAT::INPUT_FORMAT_R32G32B32A32_FLOAT, 0, 28, INPUT_CLASSIFICATION::INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "Normal", 0, INPUT_FORMAT::INPUT_FORMAT_R32G32B32A32_FLOAT, 0, 44, INPUT_CLASSIFICATION::INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, INPUT_FORMAT::INPUT_FORMAT_R32G32_FLOAT, 0, 60, INPUT_CLASSIFICATION::INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	fShaderManager->AddShader(L"..\\FuChenEngine\\Shaders\\color.hlsl");
	fShaderManager->GetShaderMap()[L"..\\FuChenEngine\\Shaders\\color.hlsl"].SetShaderLayout(mInputLayout);
	fShaderManager->AddShader(L"..\\FuChenEngine\\Shaders\\Shadows.hlsl");
	fShaderManager->GetShaderMap()[L"..\\FuChenEngine\\Shaders\\Shadows.hlsl"].SetShaderLayout(mInputLayout);
	fShaderManager->AddShader(L"..\\FuChenEngine\\Shaders\\bloomsetup.hlsl");
	fShaderManager->AddShader(L"..\\FuChenEngine\\Shaders\\bloomdown.hlsl");
	fShaderManager->AddShader(L"..\\FuChenEngine\\Shaders\\bloomup.hlsl");
	fShaderManager->AddShader(L"..\\FuChenEngine\\Shaders\\bloomsunmergeps.hlsl");
	fShaderManager->AddShader(L"..\\FuChenEngine\\Shaders\\tonemapps.hlsl");
	mInputLayout.clear();
	mInputLayout =
	{
		{ "POSITION", 0, INPUT_FORMAT::INPUT_FORMAT_R32G32B32_FLOAT, 0, 0, INPUT_CLASSIFICATION::INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	fShaderManager->GetShaderMap()[L"..\\FuChenEngine\\Shaders\\bloomsetup.hlsl"].SetShaderLayout(mInputLayout);
	fShaderManager->GetShaderMap()[L"..\\FuChenEngine\\Shaders\\bloomdown.hlsl"].SetShaderLayout(mInputLayout);
	fShaderManager->GetShaderMap()[L"..\\FuChenEngine\\Shaders\\bloomup.hlsl"].SetShaderLayout(mInputLayout);
	fShaderManager->GetShaderMap()[L"..\\FuChenEngine\\Shaders\\bloomsunmergeps.hlsl"].SetShaderLayout(mInputLayout);
	fShaderManager->GetShaderMap()[L"..\\FuChenEngine\\Shaders\\tonemapps.hlsl"].SetShaderLayout(mInputLayout);

	rhi = RHI::Get();
	rhi->Init(fShaderManager);
	rhi->CreateRenderTarget(mShadowMap, 2048.0f, 2048.0f);
	rhi->CreateRenderTarget(mSceneColorRT, 1440.0f, 900.0f);

	mBloomPP = std::make_shared<FBloomPP>(4, mSceneColorRT);
	mBloomPP->InitBloomRTs();

	rhi->InitShadowRT(mShadowMap);
 	rhi->InitPPRT(mSceneColorRT, RESOURCE_FORMAT::FORMAT_R16G16B16A16_FLOAT);
}

void ForwardRenderer::Destroy()
{
	rhi->Destroy();
	rhi->ReleaseRHI();
}

void ForwardRenderer::Render()
{
	rhi->BeginRender("geo_pso");
	//Draw ShadowMap
	ShadowPass();

	//Draw to SceneColor
	SceneColorPass();

	//PostProcess pass
	PostProcessPass(POST_PROCESS_TYPE::Bloom);

	//ToneMapps
	ToneMappsPass();

	//--------------------------------
// 	rhi->TransCurrentBackBufferResourBarrier(1, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET);
// 	rhi->SetRenderTargets(1, rhi->GetCurrentBackBufferViewHandle(), false, rhi->GetDepthStencilViewHandle());
// 	rhi->DrawPrimitives(fRenderScene, mShadowMap, mSceneColorRT);
// 	rhi->TransCurrentBackBufferResourBarrier(1, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT);
	
	rhi->EndDraw();
}

void ForwardRenderer::BuildDirtyPrimitive(FScene& fScene)
{
	rhi->PrepareForRender("geo_pso");

	std::unordered_map<std::string, FActor> allActorMap = fScene.GetAllActor();
	for (std::string renderActorName : fScene.GetDirtyActor())
	{
		rhi->TransActorToRenderPrimitive(allActorMap[renderActorName], fRenderScene);
		if (!fRenderScene.ContainTexture(allActorMap[renderActorName].GetMainTexName()))
		{
			rhi->TransTextureToRenderResource(
				allActorMap[renderActorName],
				FAssetManager::GetInstance().GetTextureFilePathFromName(allActorMap[renderActorName].GetMainTexName()),
				fRenderScene);
		}
		if (!fRenderScene.ContainTexture(allActorMap[renderActorName].GetNormalTexName()))
		{
			rhi->TransTextureToRenderResource(
				allActorMap[renderActorName],
				FAssetManager::GetInstance().GetTextureFilePathFromName(allActorMap[renderActorName].GetNormalTexName()),
				fRenderScene);
		}
		fRenderScene.GetPrimitive(renderActorName).SetMainRsvIndex(fRenderScene.GetTexByName(allActorMap[renderActorName].GetMainTexName())->GetSrvIndex());
		fRenderScene.GetPrimitive(renderActorName).SetNormalRsvIndex(fRenderScene.GetTexByName(allActorMap[renderActorName].GetNormalTexName())->GetSrvIndex());
		fScene.EraseDirtyActorByIndex(0);
	}
	rhi->EndPrepare();

	//Before render, to upload MVP buffer data to GPU.
	for (auto primitiveMap : fRenderScene.GetAllPrimitives())
	{
		auto primitive = primitiveMap.second;
		rhi->UpdateM(*primitive);
	}
	rhi->UpdateVP();
	rhi->UploadMaterialData();
}

void ForwardRenderer::ShadowPass()
{
	rhi->TransResourBarrier(mShadowMap->DSResource().get(), 1, RESOURCE_STATE_GENERIC_READ, RESOURCE_STATE_DEPTH_WRITE);
	rhi->SetRenderTargets(0, 0, false, mShadowMap->Dsv());
	rhi->BeginDraw(mShadowMap, "ShadowPass", true);
	for (auto primitivePair : fRenderScene.GetAllPrimitives())
	{
		rhi->SetPrimitive("shadow_pso", primitivePair.second);
		rhi->UploadResourceBuffer(0, primitivePair.second->GetObjCBIndex());
		rhi->UploadResourceBuffer(1, 2, -1, -1);
		rhi->DrawFPrimitive(*primitivePair.second);
	}
	rhi->EndPass();
	rhi->TransResourBarrier(mShadowMap->DSResource().get(), 1, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_GENERIC_READ);
}

void ForwardRenderer::SceneColorPass()
{
	rhi->TransResourBarrier(mSceneColorRT->ColorResource(0).get(), 1, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET);
	rhi->TransResourBarrier(mSceneColorRT->DSResource().get(), 1, RESOURCE_STATE_GENERIC_READ, RESOURCE_STATE_DEPTH_WRITE);
	rhi->SetRenderTargets(1, mSceneColorRT->Rtv(0), false, mSceneColorRT->Dsv());
	rhi->BeginDraw(mSceneColorRT, "BasePass", false);
	for (auto primitivePair : fRenderScene.GetAllPrimitives())
	{
		rhi->SetPrimitive("hdr_geo_pso", primitivePair.second);
		rhi->UploadResourceBuffer(0, primitivePair.second->GetObjCBIndex());
		rhi->UploadResourceBuffer(1, 2, 3, 4);
		rhi->UploadResourceTable(5, primitivePair.second->GetMainRsvIndex());
		rhi->UploadResourceTable(6, primitivePair.second->GetNormalRsvIndex());
		rhi->UploadResourceTable(7, mShadowMap->GetRTDesc(RTDepthStencilBuffer).rtTexture->GetSrvIndex());
		rhi->DrawFPrimitive(*primitivePair.second);
	}
	rhi->EndPass();
	rhi->TransResourBarrier(mSceneColorRT->ColorResource(0).get(), 1, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT);
	rhi->TransResourBarrier(mSceneColorRT->DSResource().get(), 1, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_GENERIC_READ);
}

void ForwardRenderer::PostProcessPass(POST_PROCESS_TYPE ppType)
{
	switch (ppType)
	{
	case POST_PROCESS_TYPE::Bloom:
		BloomPass();
		break;
	default:
		assert(0);
		break;
	}
}

void ForwardRenderer::ToneMappsPass()
{
	rhi->TransCurrentBackBufferResourBarrier(1, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET);
	rhi->SetRenderTargets(1, rhi->GetCurrentBackBufferViewHandle(), false, rhi->GetDepthStencilViewHandle());
	rhi->ToneMapps("tonemapps_pso", mBloomPP->fPrimitive, mSceneColorRT, mBloomPP->GetBloomSunmergepsRT());
	rhi->TransCurrentBackBufferResourBarrier(1, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT);
}

void ForwardRenderer::BloomPass()
{
	//Bloom setup
 	rhi->TransResourBarrier(mBloomPP->GetBloomSetUpRT()->ColorResource(0).get(), 1, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET);
 	rhi->TransResourBarrier(mBloomPP->GetBloomSetUpRT()->DSResource().get(), 1, RESOURCE_STATE_GENERIC_READ, RESOURCE_STATE_DEPTH_WRITE);
 	rhi->SetRenderTargets(1, mBloomPP->GetBloomSetUpRT()->Rtv(0), false, mBloomPP->GetBloomSetUpRT()->Dsv());
	rhi->BeginDraw(mBloomPP->GetBloomSetUpRT(), "BloomSetUp", false);
	rhi->SetPrimitive("bloom_pso", mBloomPP->fPrimitive);
	rhi->UploadResourceTable(0, mSceneColorRT->GetRTDesc(RTColorBuffer).rtTexture->GetSrvIndex());
	FVector2DInt rtSize;
	rtSize.x = mSceneColorRT->Width();
	rtSize.y = mSceneColorRT->Height();
	rhi->UploadResourceConstants(1, 2, &rtSize, 0);
	rhi->DrawFPrimitive(*mBloomPP->fPrimitive);
	rhi->EndPass();
 	rhi->TransResourBarrier(mBloomPP->GetBloomSetUpRT()->ColorResource(0).get(), 1, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT);
 	rhi->TransResourBarrier(mBloomPP->GetBloomSetUpRT()->DSResource().get(), 1, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_GENERIC_READ);
 
 	//Bloom down
 	for (size_t i = 0; i < mBloomPP->downUpNum; i++)
 	{
 		rhi->TransResourBarrier(mBloomPP->GetBloomDownRTs()[i]->ColorResource(0).get(), 1, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET);
 		rhi->TransResourBarrier(mBloomPP->GetBloomDownRTs()[i]->DSResource().get(), 1, RESOURCE_STATE_GENERIC_READ, RESOURCE_STATE_DEPTH_WRITE);
 		rhi->SetRenderTargets(1, mBloomPP->GetBloomDownRTs()[i]->Rtv(0), false, mBloomPP->GetBloomDownRTs()[i]->Dsv());
		rhi->BeginDraw(mBloomPP->GetBloomDownRTs()[i], "BloomDown", false);
		rhi->SetPrimitive("bloom_down_pso", mBloomPP->fPrimitive);
 		if (i == 0)
 		{
			rhi->UploadResourceTable(0, mBloomPP->GetBloomSetUpRT()->GetRTDesc(RTColorBuffer).rtTexture->GetSrvIndex());
			FVector2DInt rtSize;
			rtSize.x = mBloomPP->GetBloomSetUpRT()->Width();
			rtSize.y = mBloomPP->GetBloomSetUpRT()->Height();
			rhi->UploadResourceConstants(1, 2, &rtSize, 0);
 		}
 		else
 		{
			rhi->UploadResourceTable(0, mBloomPP->GetBloomDownRTs()[i - 1]->GetRTDesc(RTColorBuffer).rtTexture->GetSrvIndex());
			FVector2DInt rtSize;
			rtSize.x = mBloomPP->GetBloomDownRTs()[i - 1]->Width();
			rtSize.y = mBloomPP->GetBloomDownRTs()[i - 1]->Height();
			rhi->UploadResourceConstants(1, 2, &rtSize, 0);
 		}
		rhi->DrawFPrimitive(*mBloomPP->fPrimitive);
		rhi->EndPass();
 		rhi->TransResourBarrier(mBloomPP->GetBloomDownRTs()[i]->ColorResource(0).get(), 1, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT);
 		rhi->TransResourBarrier(mBloomPP->GetBloomDownRTs()[i]->DSResource().get(), 1, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_GENERIC_READ);
 	}
 
 	//Bloom up
 	for (size_t i = 0; i < mBloomPP->downUpNum - 1; i++)
 	{
 		rhi->TransResourBarrier(mBloomPP->GetBloomUpRTs()[i]->ColorResource(0).get(), 1, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET);
 		rhi->TransResourBarrier(mBloomPP->GetBloomUpRTs()[i]->DSResource().get(), 1, RESOURCE_STATE_GENERIC_READ, RESOURCE_STATE_DEPTH_WRITE);
 		rhi->SetRenderTargets(1, mBloomPP->GetBloomUpRTs()[i]->Rtv(0), false, mBloomPP->GetBloomUpRTs()[i]->Dsv());
		rhi->BeginDraw(mBloomPP->GetBloomUpRTs()[i], "BloomUp", false);
		rhi->SetPrimitive("bloom_up_pso", mBloomPP->fPrimitive);
 		auto bloomDownRTNum = mBloomPP->downUpNum;
 		if (i == 0)
 		{
			rhi->UploadResourceTable(0, mBloomPP->GetBloomDownRTs()[bloomDownRTNum - 1]->GetRTDesc(RTColorBuffer).rtTexture->GetSrvIndex());
			rhi->UploadResourceTable(1, mBloomPP->GetBloomDownRTs()[bloomDownRTNum - 2]->GetRTDesc(RTColorBuffer).rtTexture->GetSrvIndex());
			FVector4Int rtSize;
			rtSize.x = mBloomPP->GetBloomUpRTs()[i]->Width();
			rtSize.y = mBloomPP->GetBloomUpRTs()[i]->Height();
			rtSize.z = 20;
			rtSize.w = 20;
			rhi->UploadResourceConstants(2, 4, &rtSize, 0);
 		}
 		else
 		{
			rhi->UploadResourceTable(0, mBloomPP->GetBloomUpRTs()[i - 1]->GetRTDesc(RTColorBuffer).rtTexture->GetSrvIndex());
			rhi->UploadResourceTable(1, mBloomPP->GetBloomDownRTs()[bloomDownRTNum - 2 - i]->GetRTDesc(RTColorBuffer).rtTexture->GetSrvIndex());
			FVector4Int rtSize;
			rtSize.x = mBloomPP->GetBloomUpRTs()[i]->Width();
			rtSize.y = mBloomPP->GetBloomUpRTs()[i]->Height();
			rtSize.z = 20;
			rtSize.w = 20;
			rhi->UploadResourceConstants(2, 4, &rtSize, 0);
 		}
		rhi->DrawFPrimitive(*mBloomPP->fPrimitive);
		rhi->EndPass();
 		rhi->TransResourBarrier(mBloomPP->GetBloomUpRTs()[i]->ColorResource(0).get(), 1, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT);
 		rhi->TransResourBarrier(mBloomPP->GetBloomUpRTs()[i]->DSResource().get(), 1, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_GENERIC_READ);
 	}

  	//Bloom sunmergeps
 	rhi->TransResourBarrier(mBloomPP->GetBloomSunmergepsRT()->ColorResource(0).get(), 1, RESOURCE_STATE_PRESENT, RESOURCE_STATE_RENDER_TARGET);
 	rhi->TransResourBarrier(mBloomPP->GetBloomSunmergepsRT()->DSResource().get(), 1, RESOURCE_STATE_GENERIC_READ, RESOURCE_STATE_DEPTH_WRITE);
 	rhi->SetRenderTargets(1, mBloomPP->GetBloomSunmergepsRT()->Rtv(0), false, mBloomPP->GetBloomSunmergepsRT()->Dsv());
	rhi->BeginDraw(mBloomPP->GetBloomSunmergepsRT(), "BloomSunmergeps", false);
	rhi->SetPrimitive("bloom_sunmergeps_pso", mBloomPP->fPrimitive);
	auto bloomDownRTNum = mBloomPP->downUpNum;
	rhi->UploadResourceTable(0, mBloomPP->GetBloomUpRTs()[mBloomPP->downUpNum - 2]->GetRTDesc(RTColorBuffer).rtTexture->GetSrvIndex());
	rhi->UploadResourceTable(1, mBloomPP->GetBloomSetUpRT()->GetRTDesc(RTColorBuffer).rtTexture->GetSrvIndex());
	FVector4Int rtSizeVec4;
	rtSizeVec4.x = mBloomPP->GetBloomSunmergepsRT()->Width();
	rtSizeVec4.y = mBloomPP->GetBloomSunmergepsRT()->Height();
	rtSizeVec4.z = 20;
	rtSizeVec4.w = 20;
	rhi->UploadResourceConstants(2, 4, &rtSizeVec4, 0);
	rhi->DrawFPrimitive(*mBloomPP->fPrimitive);
	rhi->EndPass();
 	rhi->TransResourBarrier(mBloomPP->GetBloomSunmergepsRT()->ColorResource(0).get(), 1, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_PRESENT);
 	rhi->TransResourBarrier(mBloomPP->GetBloomSunmergepsRT()->DSResource().get(), 1, RESOURCE_STATE_DEPTH_WRITE, RESOURCE_STATE_GENERIC_READ);
}
