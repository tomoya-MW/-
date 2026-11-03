#include "DxLib.h"

// 各キャラの中心を基準に描画するための補正関数。
void DrawGra(int x, int y, int GrHandle, int TransFlag)
{
	int w, h;
	GetGraphSize(GrHandle, &w, &h);
	DrawGraph(x-w/2, y-h/2, GrHandle, TransFlag);
}
