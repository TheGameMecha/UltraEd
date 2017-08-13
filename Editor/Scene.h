#ifndef _SCENE_H_
#define _SCENE_H_

// Ignore warnings generated by using a GUID with std::map.
#pragma warning(disable: 4786)

#include <map>
#include <d3d8.h>
#include <d3dx8.h>
#include "cJSON.h"
#include "Camera.h"
#include "Common.h"
#include "Debug.h"
#include "Gizmo.h"
#include "Grid.h"
#include "Model.h"
#include "Savable.h"

class CScene : public CSavable
{
public:
  CScene();
  ~CScene();
  char* Save();
  BOOL Create(HWND windowHandle);
  void Delete();
  void Render();
  void Resize(int width, int height);
  void OnMouseWheel(short zDelta);
  void OnNew();
  void OnSave();
  void OnApplyTexture();
  void OnImportModel();
  BOOL Pick(POINT mousePoint);
  void ReleaseResources(ModelRelease type);
  void CheckInput(float);
  void ScreenRaycast(POINT screenPoint,
    D3DXVECTOR3 *origin, D3DXVECTOR3 *dir);
  void SetGizmoModifier(GizmoModifierState state);
  bool ToggleMovementSpace();
  
private:
  HWND m_hWnd;
  D3DLIGHT8 m_worldLight;
  D3DMATERIAL8 m_defaultMaterial;
  CGizmo m_gizmo;
  CCamera m_camera;
  IDirect3DDevice8* m_device;
  IDirect3D8* m_d3d8;
  D3DPRESENT_PARAMETERS m_d3dpp;
  std::map<GUID, CModel> m_models;
  CGrid m_grid;
  GUID m_selectedModelId;
  float mouseSmoothX, mouseSmoothY;
};

#endif