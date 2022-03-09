#pragma once
#include "FInputBase.h"

class FWin32Input
{
public:
	FWin32Input();
	~FWin32Input();
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static FWin32Input* GetFWin32Input();
	void OnKeyboardInput(const GameTimer& gt);
	void Update(const GameTimer& gt);

protected:
	static std::unique_ptr<FWin32Input> fWin32Input;

private:
	virtual void OnMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
	
	POINT mLastMousePos;
};