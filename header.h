#pragma once
#pragma comment(lib, "Ws2_32.lib")

// 4514 : �Q�Ƃ���Ă��Ȃ��C�����C���֐��͍폜����܂����B
// 4626 : ������Z�q�͈ÖٓI�ɍ폜�ς݂Ƃ��Ē�`����܂����B
// 4820 : ?? �o�C�g�̃p�f�B���O�� �f�[�^ �����o�[ ?? �̌�ɒǉ����܂����B
// 5039 : �X���[����\��������֐��ւ̃|�C���^�[�܂��͎Q�Ƃ��A-EHc �� 'extern "C"' �֐��ɓn����܂����B
//        ���̊֐�����O���X���[����ƁA����`�̓��삪��������\��������܂��B
// 5045 : /Qspectre �X�C�b�`���w�肳��Ă���Ƃ��ASpectre �̌y�����}�����܂��B
#pragma warning( disable: 4514 4626 4820 5039 5045 )

#define WIN32_LEAN_AND_MEAN

#pragma warning( push )
// 4365 : '��������': 'int' ���� 'UINT8' �ɕϊ����܂����Bsigned/unsigned ����v���܂���
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


