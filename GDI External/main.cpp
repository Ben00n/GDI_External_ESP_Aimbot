#include "Offsets.h"
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>

/* SexOffenderSally helped code a lot/most of this with me, a long time friend! Give him a <3 on discord
Make sure character set is 'Multi-Byte' in project settings! And game must be windowed fullscreen.
Updated offsets: https://github.com/frk1/hazedumper/blob/master/csgo.cs     */

#define EnemyPen 0x000000FF
HBRUSH EnemyBrush = CreateSolidBrush(0x000000FF);

const int SCREEN_WIDTH = GetSystemMetrics(SM_CXSCREEN); const int xhairx = SCREEN_WIDTH / 2;
const int SCREEN_HEIGHT = GetSystemMetrics(SM_CYSCREEN); const int xhairy = SCREEN_HEIGHT / 2;

int closest; //Used in a thread to save CPU usage.

class Vector3 {
public:
	float x, y, z;
	Vector3() : x(0.f), y(0.f), z(0.f) {}
	Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};

struct view_matrix_t {
	float matrix[16];
} vm;

uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName)
{
	uintptr_t modBaseAddr = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		MODULEENTRY32 modEntry;
		modEntry.dwSize = sizeof(modEntry);
		if (Module32First(hSnap, &modEntry))
		{
			do
			{
				if (!_wcsicmp(modEntry.szModule, modName))
				{
					modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
					break;
				}
			} while (Module32Next(hSnap, &modEntry));
		}
	}
	CloseHandle(hSnap);
	return modBaseAddr;
}

DWORD GetProcId(const wchar_t* procName)
{
	DWORD procId = 0;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap != INVALID_HANDLE_VALUE)
	{
		PROCESSENTRY32 procEntry;
		procEntry.dwSize = sizeof(procEntry);

		if (Process32First(hSnap, &procEntry))
		{
			do
			{
				if (!_wcsicmp(procEntry.szExeFile, procName))
				{
					procId = procEntry.th32ProcessID;
					break;
				}
			} while (Process32Next(hSnap, &procEntry));

		}
	}
	CloseHandle(hSnap);
	return procId;
}

HWND hwnd;
DWORD procId;
uintptr_t moduleBase;
HANDLE hProcess;
HDC hdc;

template<typename T> T RPM(SIZE_T address) {
	//The buffer for data that is going to be read from memory
	T buffer;

	//The actual RPM
	ReadProcessMemory(hProcess, (LPCVOID)address, &buffer, sizeof(T), NULL);

	//Return our buffer
	return buffer;
}

struct Vector3 WorldToScreen(const struct Vector3 pos, struct view_matrix_t matrix) { //This turns 3D coordinates (ex: XYZ) int 2D coordinates (ex: XY).
	struct Vector3 out;
	float _x = matrix.matrix[0] * pos.x + matrix.matrix[1] * pos.y + matrix.matrix[2] * pos.z + matrix.matrix[3];
	float _y = matrix.matrix[4] * pos.x + matrix.matrix[5] * pos.y + matrix.matrix[6] * pos.z + matrix.matrix[7];
	out.z = matrix.matrix[12] * pos.x + matrix.matrix[13] * pos.y + matrix.matrix[14] * pos.z + matrix.matrix[15];

	_x *= 1.f / out.z;
	_y *= 1.f / out.z;

	out.x = SCREEN_WIDTH * .5f;
	out.y = SCREEN_HEIGHT * .5f;

	out.x += 0.5f * _x * SCREEN_WIDTH + 0.5f;
	out.y -= 0.5f * _y * SCREEN_HEIGHT + 0.5f;

	return out;
}

int getTeam(uintptr_t player)
{
	return RPM<int>(player + hazedumper::netvars::m_iTeamNum);
}

uintptr_t GetLocalPlayer()
{
	return RPM< uintptr_t>(moduleBase + hazedumper::signatures::dwLocalPlayer);
}

uintptr_t GetPlayer(int index)
{  //Each player has an index. 1-64
	return RPM< uintptr_t>(moduleBase + hazedumper::signatures::dwEntityList + index * 0x10); //We multiply the index by 0x10 to select the player we want in the entity list.
}

int GetPlayerHealth(uintptr_t player)
{
	return RPM<int>(player + hazedumper::netvars::m_iHealth);
}

Vector3 PlayerLocation(uintptr_t player)
{ //Stores XYZ coordinates in a Vector3.
	return RPM<Vector3>(player + hazedumper::netvars::m_vecOrigin);
}

bool DormantCheck(uintptr_t player) {
	return RPM<int>(player + hazedumper::signatures::m_bDormant);
}

Vector3 get_head(uintptr_t player) {
	struct boneMatrix_t {
		byte pad3[12];
		float x;
		byte pad1[12];
		float y;
		byte pad2[12];
		float z;
	};
	uintptr_t boneBase = RPM<uintptr_t>(player + hazedumper::netvars::m_dwBoneMatrix);
	boneMatrix_t boneMatrix = RPM<boneMatrix_t>(boneBase + (sizeof(boneMatrix) * 8 /*8 is the boneid for head*/));
	return Vector3(boneMatrix.x, boneMatrix.y, boneMatrix.z);
}

float pythag(int x1, int y1, int x2, int y2) {
	return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

int FindClosestEnemy() {
	float Finish;
	int ClosestEntity = 1;
	Vector3 Calc = { 0, 0, 0 };
	float Closest = FLT_MAX;
	int localTeam = getTeam(GetLocalPlayer());
	for (int i = 1; i < 64; i++) { //Loops through all the entitys in the index 1-64.
		DWORD Entity = GetPlayer(i);
		int EnmTeam = getTeam(Entity); if (EnmTeam == localTeam) continue;
		int EnmHealth = GetPlayerHealth(Entity); if (EnmHealth < 1 || EnmHealth > 100) continue;
		int Dormant = DormantCheck(Entity); if (Dormant) continue;
		Vector3 headBone = WorldToScreen(get_head(Entity), vm);
		Finish = pythag(headBone.x, headBone.y, xhairx, xhairy);
		if (Finish < Closest) {
			Closest = Finish;
			ClosestEntity = i;
		}
	}

	return ClosestEntity;
}

void FindClosestEnemyThread() {
	while (1) {
		closest = FindClosestEnemy();
	}
}

void DrawFilledRect(int x, int y, int w, int h)
{
	RECT rect = { x, y, x + w, y + h };
	FillRect(hdc, &rect, EnemyBrush);
}

void DrawBorderBox(int x, int y, int w, int h, int thickness)
{
	DrawFilledRect(x, y, w, thickness); //Top horiz line
	DrawFilledRect(x, y, thickness, h); //Left vertical line
	DrawFilledRect((x + w), y, thickness, h); //right vertical line
	DrawFilledRect(x, y + h, w + thickness, thickness); //bottom horiz line
}

void DrawLine(float StartX, float StartY, float EndX, float EndY)
{
	int a, b = 0;
	HPEN hOPen;
	HPEN hNPen = CreatePen(PS_SOLID, 2, EnemyPen);// penstyle, width, color
	hOPen = (HPEN)SelectObject(hdc, hNPen);
	MoveToEx(hdc, StartX, StartY, NULL); //start
	a = LineTo(hdc, EndX, EndY); //end
	DeleteObject(SelectObject(hdc, hOPen));
}

int main()
{
	hwnd = FindWindowA(NULL, "Counter-Strike: Global Offensive - Direct3D 9");
	GetWindowThreadProcessId(hwnd, &procId);
	moduleBase = GetModuleBaseAddress(procId, L"client.dll");
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, NULL, procId);
	hdc = GetDC(hwnd);
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)FindClosestEnemyThread, NULL, NULL, NULL);

	if (hProcess == NULL)
	{
		std::cout << "Open this while inside csgo" << std::endl;
		Sleep(3000);
		exit(1);
	}

	bool bEsp = false, bAimbot = false;
	DWORD dwExit = 0;

	std::cout << "csgo pid: " + procId << std::endl;
	std::cout << "\nF1: Toggle ESP" << std::endl;
	std::cout << "\nF2: Toggle Aimbot" << std::endl;


	while (GetExitCodeProcess(hProcess, &dwExit) && dwExit == STILL_ACTIVE)
	{
		if (GetAsyncKeyState(VK_F1) & 1)
		{
			bEsp = !bEsp;
		}
		if (GetAsyncKeyState(VK_F2) & 1)
		{
			bAimbot = !bAimbot;
		}
		view_matrix_t vm = RPM<view_matrix_t>(moduleBase + hazedumper::signatures::dwViewMatrix);

		if (bEsp)
		{
			for (int i = 1; i < 64; i++)
			{
				uintptr_t pEnt = RPM<DWORD>(moduleBase + hazedumper::signatures::dwEntityList + (i * 0x10));
				int localteam = RPM<int>(RPM<DWORD>(moduleBase + hazedumper::signatures::dwEntityList) + hazedumper::netvars::m_iTeamNum);

				int health = RPM<int>(pEnt + hazedumper::netvars::m_iHealth);
				int team = RPM<int>(pEnt + hazedumper::netvars::m_iTeamNum);

				Vector3 pos = RPM<Vector3>(pEnt + hazedumper::netvars::m_vecOrigin);
				Vector3 head;
				head.x = pos.x;
				head.y = pos.y;
				head.z = pos.z + 75.f;
				Vector3 screenpos = WorldToScreen(pos, vm);
				Vector3 screenhead = WorldToScreen(head, vm);
				float height = screenhead.y - screenpos.y;
				float width = height / 2.4f;

				if (screenpos.z >= 0.01f && team != localteam && health > 0 && health < 101) {
					DrawBorderBox(screenpos.x - (width / 2), screenpos.y, width, height, 2);
					DrawLine(SCREEN_WIDTH / 2, SCREEN_HEIGHT, screenpos.x, screenpos.y);
				}
			}
		}
		if (bAimbot)
		{
			Vector3 closestw2shead = WorldToScreen(get_head(GetPlayer(closest)), vm);
			//DrawLine(xhairx, xhairy, closestw2shead.x, closestw2shead.y); //optinal for debugging

			if (GetAsyncKeyState(VK_MENU) && closestw2shead.z >= 0.001f /*onscreen check*/)
				SetCursorPos(closestw2shead.x, closestw2shead.y + 5); //turn off "raw input" in CSGO settings
		}
	}
}