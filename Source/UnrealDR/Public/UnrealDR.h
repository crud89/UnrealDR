#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include <OpenVR/OpenVRv1_5_17/headers/openvr.h>

DEFINE_LOG_CATEGORY_STATIC(LOG_UNREAL_DR, Log, All);

class FUnrealDRModule : public IModuleInterface {
private:
	vr::IVRSystem* m_system;

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	vr::IVRSystem* getVirtualRealitySystem() const;
};
