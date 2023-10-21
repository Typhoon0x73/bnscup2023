﻿#pragma once
#ifndef BNSCUP_SCENE_DEFINE_H_
#define BNSCUP_SCENE_DEFINE_H_

#include <Siv3D.hpp>

namespace bnscup
{
	enum class SceneKey
	{
		Load,
		Title,
		StageSelect,
		Game,

		Exit,
	};

	class AssetRegister;
	class MapData;
	struct SceneData
	{
		int32 stageNo;
		SceneKey nextScene;
		AssetRegister* pAssetRegister;
		MapData* pMapData;
	};

	using GameApp = SceneManager<SceneKey, SceneData>;
}

#endif // !BNSCUP_SCENE_DEFINE_H_
