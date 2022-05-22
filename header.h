#pragma once
#pragma comment(lib, "Ws2_32.lib")

// 4514 : 参照されていないインライン関数は削除されました。
// 4626 : 代入演算子は暗黙的に削除済みとして定義されました。
// 4820 : ?? バイトのパディングを データ メンバー ?? の後に追加しました。
// 5039 : スローする可能性がある関数へのポインターまたは参照が、-EHc で 'extern "C"' 関数に渡されました。
//        この関数が例外をスローすると、未定義の動作が発生する可能性があります。
// 5045 : /Qspectre スイッチが指定されているとき、Spectre の軽減策を挿入します。
#pragma warning( disable: 4514 4626 4820 5039 5045 )

#define WIN32_LEAN_AND_MEAN

#pragma warning( push )
// 4365 : '初期化中': 'int' から 'UINT8' に変換しました。signed/unsigned が一致しません
#pragma warning( disable: 4365 )

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#else
#include <stdlib.h>
#endif

#include <windows.h>
#include <winsock2.h>
#include <iostream>

#pragma warning( pop )

#ifdef _DEBUG
#define NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#define NEW new
#endif


