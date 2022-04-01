#pragma once
#include "FRenderer.h"
#include "RHI.h"

class ForwardRenderer : public FRenderer
{
public:
	ForwardRenderer();
	virtual ~ForwardRenderer();

	virtual void Init()override;
	virtual void Destroy()override;
	virtual void Render()override;
	virtual void BuildDirtyPrimitive(FScene& fScene)override;

private:
	bool testInitTextureOnce = true;
	RHI* rhi;
	FRenderScene fRenderScene;
};