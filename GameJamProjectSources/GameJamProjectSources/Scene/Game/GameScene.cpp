﻿#include "GameScene.h"
#include "../../Common/Common.h"
#include "Map/MapData.h"
#include "Map/MapView.h"
#include "Map/RoomData.h"
#include "Pause/PauseView.h"
#include "../../Unit/Unit.h"
#include "../../Unit/Enemy.h"
#include "../../Item/Item.h"
#include "../../Button/Button.h"
#include "../../MessageBox/MessageBox.h"
#include "../../TeleportAnim/TeleportAnim.h"

namespace
{
	static const RoundRect ROUNDRECT_STAGENO_AREA =
	{
		20, 20,
		1240, 80, 10
	};

	static const RoundRect ROUNDRECT_MAPVIEW_AREA =
	{
		20, 120,
		1240, 600, 10
	};

	static const RoundRect ROUNDRECT_LOG_AREA =
	{
		20, 740,
		1020, 200, 10
	};

	static const Circle CIRCLE_CONTROLLER_AREA =
	{
		1160, 840, 100
	};

	static const Circle CIRCLE_CONTROLLER_UP_AREA =
	{
		1160, 770, 40
	};

	static const Circle CIRCLE_CONTROLLER_DOWN_AREA =
	{
		1160, 900, 40
	};

	static const Circle CIRCLE_CONTROLLER_LEFT_AREA =
	{
		1090, 840, 40
	};

	static const Circle CIRCLE_CONTROLLER_RIGHT_AREA =
	{
		1230, 840, 40
	};

	static const RectF RECT_EXIT_BUTTON =
	{
		1120, 670, 80, 40,
	};

	static const RectF RECT_PAUSE_BUTTON =
	{
		1120, 30, 120, 60,
	};

	// 描画された最大のアルファ成分を保持するブレンドステートを作成する
	static const BlendState MakeBlendState()
	{
		BlendState blendState = BlendState::Default2D;
		blendState.srcAlpha = Blend::SrcAlpha;
		blendState.dstAlpha = Blend::DestAlpha;
		blendState.opAlpha = BlendOp::Max;
		return blendState;
	}

	Vec2 MapPosToGlobalPos(const Point& mapPos)
	{
		const int32 chipSize = 16;
		const SizeF roomSize{ chipSize * 5, chipSize * 5 };
		return Vec2{ roomSize.x * 0.5 + roomSize.x * mapPos.x, roomSize.y * 0.5 + roomSize.y * mapPos.y };
	}

	Point MapPosFromGlobalPos(const Vec2& pos)
	{
		const int32 chipSize = 16;
		const SizeF roomSize{ chipSize * 5, chipSize * 5 };
		return Point{
			static_cast<int32>(pos.x - roomSize.x * 0.5) / static_cast<int32>(roomSize.x),
			static_cast<int32>(pos.y - roomSize.y * 0.5) / static_cast<int32>(roomSize.y)
		};
	}
}

namespace bnscup
{
	class GameScene::Impl
	{
		enum class Step
		{
			Assign,
			Idle,
			Move,
			Pause,
			RescueAnim,
			ReturnAnim,
			UseKeyPopup,
			RescuePopup,
			ReturnPopup,
			CommonPopup,
			GameOver,
			Result,
			End,
		};

		struct UnlockRoomData
		{
			RoomData* pTargetRoom;
			RoomData::Route unlockRoute;
		};

		using Rescued = YesNo<struct Rescued_tag>;
		struct RescueUnitData
		{
			Rescued rescued;
			Unit* pTargetUnit;
		};


	public:

		Impl(int stageNo);
		~Impl();

		void update();
		void draw() const;

		bool isEnd() const;
		SceneKey getNextScene() const;

	private:

		void stepAssign();
		void stepIdle();
		void stepMove();
		void stepPause();
		void stepRescueAnim();
		void stepReturnAnim();
		void stepUseKeyPopup();
		void stepRescuePopup();
		void stepReturnPopup();
		void stepCommonPopup();
		void stepGameOver();
		void stepResult();

		void createUseKeyPopup();
		void createRescuePopup();
		void createReturnPopup();
		void createNotHaveKeyPopup();

		void enemyMove();
		void checkEnemyMoveDir(Enemy* pEnemy);

	private:

		SceneKey m_nextScene;

		Step m_step;
		Camera2D m_camera;
		String m_stageNoText;
		std::unique_ptr<MapData> m_pMapData;
		std::unique_ptr<MapView> m_pMapView;
		RenderTexture m_renderTarget;

		Unit* m_pPlayerUnit;
		Array<std::unique_ptr<Unit>> m_units;
		Array<RescueUnitData> m_targetUnits;
		Array<Enemy*> m_enemies;

		Array<Item*> m_holdKeys;
		Array<std::unique_ptr<Item>> m_items;

		Texture m_controllerTexture;
		Button m_controlButtons[4];
		Button m_exitButton;
		Button m_pauseButton;
		PauseView m_pauseView;

		std::unique_ptr<MessageBox> m_pMessageBox;
		Optional<UnlockRoomData> m_unlockRoomData;
		RescueUnitData* m_pRescueUnitTarget;

		TeleportAnim m_teleportAnim;

		Font m_buttonFont;
		Audio m_collectItemSE;
		Audio m_unlockDoorSE;
		Audio m_ingameBGM;
	};

	//==================================================
	
	GameScene::Impl::Impl(int stageNo)
		: m_nextScene{ SceneKey::Title }
		, m_step{ Step::Idle }
		, m_camera{ Vec2::Zero(), 1.0, Camera2DParameters::NoControl() }
		, m_stageNoText{ U"" }
		, m_pMapData{ nullptr }
		, m_pMapView{ nullptr }
		, m_renderTarget{
			static_cast<uint32>(ROUNDRECT_MAPVIEW_AREA.rect.size.x)
			, static_cast<uint32>(ROUNDRECT_MAPVIEW_AREA.rect.size.y) }
		, m_pPlayerUnit{ nullptr }
		, m_units{}
		, m_targetUnits{}
		, m_enemies{}
		, m_holdKeys{}
		, m_items{}
		, m_controllerTexture{}
		, m_controlButtons{
			Button(CIRCLE_CONTROLLER_UP_AREA)
			, Button(CIRCLE_CONTROLLER_DOWN_AREA)
			, Button(CIRCLE_CONTROLLER_LEFT_AREA)
			, Button(CIRCLE_CONTROLLER_RIGHT_AREA) }
		, m_exitButton{ RECT_EXIT_BUTTON }
		, m_pauseButton{ RECT_PAUSE_BUTTON }
		, m_pauseView{}
		, m_pMessageBox{ nullptr }
		, m_unlockRoomData{ none }
		, m_pRescueUnitTarget{ nullptr }
		, m_teleportAnim{}
		, m_buttonFont{}
		, m_collectItemSE{}
		, m_unlockDoorSE{}
		, m_ingameBGM{}
	{
		// ステージ表示用
		m_stageNoText = U"ステージ{}"_fmt(stageNo + 1);

		// あとでステージ生成クラスとかにまとめたい
		if (stageNo == 0)
		{
			const int32 chipSize = 16;
			const int32 roomCountX = 2;
			const int32 roomCountY = 3;

			// マップの生成
			auto* pMapData = new MapData(
				{
					RoomData{ FromEnum(RoomData::Route::Down)       , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::None), FromEnum(RoomData::Route::None) },
					RoomData{ FromEnum(RoomData::Route::UpRightDown), FromEnum(RoomData::Route::Up)   }, RoomData{ FromEnum(RoomData::Route::Left), FromEnum(RoomData::Route::None) },
					RoomData{ FromEnum(RoomData::Route::Up)         , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::None), FromEnum(RoomData::Route::None) },
				},
				U"dungeon_tileset",
				roomCountX, roomCountY, chipSize
			);
			m_pMapData.reset(pMapData);

			// 救助対象ユニット
			{
				auto* pUnit = new Unit();
				pUnit->setPos(MapPosToGlobalPos(Point{ 0, 0 }));
				pUnit->setTexture(U"dungeon_tileset_2");
				pUnit->setAnimRect(
					{
						{ 0.175s, RectF{ 128, 256, 16, 32 } },
						{ 0.175s, RectF{ 144, 256, 16, 32 } },
						{ 0.175s, RectF{ 160, 256, 16, 32 } },
						{ 0.175s, RectF{ 176, 256, 16, 32 } },
					}
				);
				m_targetUnits.emplace_back(Rescued::No, pUnit);
				m_units.emplace_back(pUnit);
			}

			const Point startRoom{ 0, 2 };

			// プレイヤーの生成
			{
				auto* pPlayerUnit = new Unit();
				pPlayerUnit->setPos(MapPosToGlobalPos(startRoom));
				pPlayerUnit->setTexture(U"dungeon_tileset_2");
				pPlayerUnit->setFootStepSE(U"sd_foot_step");
				pPlayerUnit->setAnimRect(
					{
						{ 0.2s, RectF{ 128, 64, 16, 32 } },
						{ 0.2s, RectF{ 144, 64, 16, 32 } },
						{ 0.2s, RectF{ 160, 64, 16, 32 } },
						{ 0.2s, RectF{ 176, 64, 16, 32 } },
					}
				);
				m_pPlayerUnit = pPlayerUnit;
				m_units.emplace_back(pPlayerUnit);
			}

			// アイテムの生成
			{
				const Point keyPos{ 1, 1 };
				auto* pItem = new Item(Item::Type::GoldKey);
				pItem->setPos(MapPosToGlobalPos(keyPos));
				pItem->setSrcRect(RectF{ 9 * chipSize, 9 * chipSize, chipSize, chipSize });
				pItem->setTexture(U"dungeon_tileset");
				m_items.emplace_back(pItem);
			}

		}
		else if (stageNo == 1)
		{
			const int32 chipSize = 16;
			const int32 roomCountX = 3;
			const int32 roomCountY = 3;

			// マップの生成
			auto* pMapData = new MapData(
				{
					RoomData{ FromEnum(RoomData::Route::Down)       , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::Right)        , FromEnum(RoomData::Route::Right) }, RoomData{ FromEnum(RoomData::Route::DownLeft)  , FromEnum(RoomData::Route::None) },
					RoomData{ FromEnum(RoomData::Route::UpRightDown), FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::RightDownLeft), FromEnum(RoomData::Route::None)  }, RoomData{ FromEnum(RoomData::Route::UpDownLeft), FromEnum(RoomData::Route::None) },
					RoomData{ FromEnum(RoomData::Route::UpRight)    , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpRightLeft)  , FromEnum(RoomData::Route::None)  }, RoomData{ FromEnum(RoomData::Route::UpLeft)    , FromEnum(RoomData::Route::None) },
				},
				U"dungeon_tileset",
				roomCountX, roomCountY, chipSize
				);
			m_pMapData.reset(pMapData);

			// 救助対象ユニット
			{
				auto* pUnit = new Unit();
				pUnit->setPos(MapPosToGlobalPos(Point{ 1, 0 }));
				pUnit->setTexture(U"dungeon_tileset_2");
				pUnit->setAnimRect(
					{
						{ 0.175s, RectF{ 128, 256, 16, 32 } },
						{ 0.175s, RectF{ 144, 256, 16, 32 } },
						{ 0.175s, RectF{ 160, 256, 16, 32 } },
						{ 0.175s, RectF{ 176, 256, 16, 32 } },
					}
				);
				m_targetUnits.emplace_back(Rescued::No, pUnit);
				m_units.emplace_back(pUnit);
			}

			const Point startRoom{ 0, 1 };

			// プレイヤーの生成
			{
				auto* pPlayerUnit = new Unit();
				pPlayerUnit->setPos(MapPosToGlobalPos(startRoom));
				pPlayerUnit->setTexture(U"dungeon_tileset_2");
				pPlayerUnit->setFootStepSE(U"sd_foot_step");
				pPlayerUnit->setAnimRect(
					{
						{ 0.2s, RectF{ 128, 64, 16, 32 } },
						{ 0.2s, RectF{ 144, 64, 16, 32 } },
						{ 0.2s, RectF{ 160, 64, 16, 32 } },
						{ 0.2s, RectF{ 176, 64, 16, 32 } },
					}
				);
				m_pPlayerUnit = pPlayerUnit;
				m_units.emplace_back(pPlayerUnit);
			}

			// 敵の生成
			{
				auto* pEnemy = new Enemy();
				pEnemy->setPos(MapPosToGlobalPos(Point{ 2, 0 }));
				pEnemy->setMoveType(Enemy::MoveType::UpDown);
				pEnemy->setTexture(U"dungeon_tileset_2");
				pEnemy->setAnimRect(
					{
						{ 0.15s, RectF{ 368, 272, 16, 24 } },
						{ 0.15s, RectF{ 384, 272, 16, 24 } },
						{ 0.15s, RectF{ 400, 272, 16, 24 } },
						{ 0.15s, RectF{ 416, 272, 16, 24 } },
					}
				);
				m_enemies.emplace_back(pEnemy);
				m_units.emplace_back(pEnemy);
			}

			// アイテムの生成
			{
				const Point keyPos{ 1, 2 };
				auto* pItem = new Item(Item::Type::GoldKey);
				pItem->setPos(MapPosToGlobalPos(keyPos));
				pItem->setSrcRect(RectF{ 9 * chipSize, 9 * chipSize, chipSize, chipSize });
				pItem->setTexture(U"dungeon_tileset");
				m_items.emplace_back(pItem);
			}

		}
		else if (stageNo == 2)
		{
			const int32 chipSize = 16;
			const int32 roomCountX = 6;
			const int32 roomCountY = 5;

			// マップの生成
			auto* pMapData = new MapData(
				{
					RoomData{ FromEnum(RoomData::Route::Right)      , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::RightDownLeft), FromEnum(RoomData::Route::Left) }, RoomData{ FromEnum(RoomData::Route::RightDownLeft), FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::RightLeft)    , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::DownLeft)   , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::Down)      , FromEnum(RoomData::Route::None) },
					RoomData{ FromEnum(RoomData::Route::RightDown)  , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpRightLeft)  , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::All)          , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::DownLeft)     , FromEnum(RoomData::Route::Down) }, RoomData{ FromEnum(RoomData::Route::UpRightDown), FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpDownLeft), FromEnum(RoomData::Route::None) },
					RoomData{ FromEnum(RoomData::Route::UpRightDown), FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::RightDownLeft), FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpDownLeft)   , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpRight)      , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpDownLeft) , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpDown)    , FromEnum(RoomData::Route::None) },
					RoomData{ FromEnum(RoomData::Route::UpDown)     , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpRightDown)  , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::All)          , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::RightDownLeft), FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpRightLeft), FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpDownLeft), FromEnum(RoomData::Route::None) },
					RoomData{ FromEnum(RoomData::Route::UpRight)    , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpRightLeft)  , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpRightLeft)  , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpLeft)       , FromEnum(RoomData::Route::Left) }, RoomData{ FromEnum(RoomData::Route::Right)      , FromEnum(RoomData::Route::None) }, RoomData{ FromEnum(RoomData::Route::UpLeft)    , FromEnum(RoomData::Route::None) },
				},
				U"dungeon_tileset",
				roomCountX, roomCountY, chipSize
				);
			m_pMapData.reset(pMapData);

			// 救助対象ユニット
			{
				auto* pUnit = new Unit();
				pUnit->setPos(MapPosToGlobalPos(Point{ 0, 0 }));
				pUnit->setTexture(U"dungeon_tileset_2");
				pUnit->setAnimRect(
					{
						{ 0.175s, RectF{ 128, 256, 16, 32 } },
						{ 0.175s, RectF{ 144, 256, 16, 32 } },
						{ 0.175s, RectF{ 160, 256, 16, 32 } },
						{ 0.175s, RectF{ 176, 256, 16, 32 } },
					}
				);
				m_targetUnits.emplace_back(Rescued::No, pUnit);
				m_units.emplace_back(pUnit);
			}
			{
				auto* pUnit = new Unit();
				pUnit->setPos(MapPosToGlobalPos(Point{ 3, 4 }));
				pUnit->setTexture(U"dungeon_tileset_2");
				pUnit->setAnimRect(
					{
						{ 0.175s, RectF{ 128, 288, 16, 32 } },
						{ 0.175s, RectF{ 144, 288, 16, 32 } },
						{ 0.175s, RectF{ 160, 288, 16, 32 } },
						{ 0.175s, RectF{ 176, 288, 16, 32 } },
					}
				);
				m_targetUnits.emplace_back(Rescued::No, pUnit);
				m_units.emplace_back(pUnit);
			}
			{
				auto* pUnit = new Unit();
				pUnit->setPos(MapPosToGlobalPos(Point{ 3, 2 }));
				pUnit->setTexture(U"dungeon_tileset_2");
				pUnit->setAnimRect(
					{
						{ 0.175s, RectF{ 128, 32, 16, 32 } },
						{ 0.175s, RectF{ 144, 32, 16, 32 } },
						{ 0.175s, RectF{ 160, 32, 16, 32 } },
						{ 0.175s, RectF{ 176, 32, 16, 32 } },
					}
				);
				m_targetUnits.emplace_back(Rescued::No, pUnit);
				m_units.emplace_back(pUnit);
			}

			// アイテムの生成
			const Point KeyPositions[] =
			{
				{ 0, 1 },
				{ 0, 4 },
				{ 5, 0 },
			};
			for (const auto& keyPos : KeyPositions)
			{
				auto* pItem = new Item(Item::Type::GoldKey);
				pItem->setPos(MapPosToGlobalPos(keyPos));
				pItem->setSrcRect(RectF{ 9 * chipSize, 9 * chipSize, chipSize, chipSize });
				pItem->setTexture(U"dungeon_tileset");
				m_items.emplace_back(pItem);
			}

			// 敵の生成
			{
				auto* pEnemy = new Enemy();
				pEnemy->setPos(MapPosToGlobalPos(Point{ 4, 0 }));
				pEnemy->setMoveType(Enemy::MoveType::UpDown);
				pEnemy->setTexture(U"dungeon_tileset_2");
				pEnemy->setAnimRect(
					{
						{ 0.15s, RectF{ 368, 272, 16, 24 } },
						{ 0.15s, RectF{ 384, 272, 16, 24 } },
						{ 0.15s, RectF{ 400, 272, 16, 24 } },
						{ 0.15s, RectF{ 416, 272, 16, 24 } },
					}
				);
				m_enemies.emplace_back(pEnemy);
				m_units.emplace_back(pEnemy);
			}
			{
				auto* pEnemy = new Enemy();
				pEnemy->setPos(MapPosToGlobalPos(Point{ 1, 0 }));
				pEnemy->setMoveType(Enemy::MoveType::LeftRight);
				pEnemy->setMoveDirection(RoomData::Route::Right);
				pEnemy->setTexture(U"dungeon_tileset_2");
				pEnemy->setAnimRect(
					{
						{ 0.15s, RectF{ 368, 248, 16, 24 } },
						{ 0.15s, RectF{ 384, 248, 16, 24 } },
						{ 0.15s, RectF{ 400, 248, 16, 24 } },
						{ 0.15s, RectF{ 416, 248, 16, 24 } },
					}
				);
				m_enemies.emplace_back(pEnemy);
				m_units.emplace_back(pEnemy);
			}
			{
				auto* pEnemy = new Enemy();
				pEnemy->setPos(MapPosToGlobalPos(Point{ 5, 3 }));
				pEnemy->setMoveType(Enemy::MoveType::LeftRight);
				pEnemy->setMoveDirection(RoomData::Route::Left);
				pEnemy->setMirror(true);
				pEnemy->setTexture(U"dungeon_tileset_2");
				pEnemy->setAnimRect(
					{
						{ 0.15s, RectF{ 368, 248, 16, 24 } },
						{ 0.15s, RectF{ 384, 248, 16, 24 } },
						{ 0.15s, RectF{ 400, 248, 16, 24 } },
						{ 0.15s, RectF{ 416, 248, 16, 24 } },
					}
				);
				m_enemies.emplace_back(pEnemy);
				m_units.emplace_back(pEnemy);
			}

			const Point startRoom{ 2, 2 };

			// プレイヤーの生成
			{
				auto* pPlayerUnit = new Unit();
				pPlayerUnit->setPos(MapPosToGlobalPos(startRoom));
				pPlayerUnit->setTexture(U"dungeon_tileset_2");
				pPlayerUnit->setFootStepSE(U"sd_foot_step");
				pPlayerUnit->setAnimRect(
					{
						{ 0.2s, RectF{ 128, 64, 16, 32 } },
						{ 0.2s, RectF{ 144, 64, 16, 32 } },
						{ 0.2s, RectF{ 160, 64, 16, 32 } },
						{ 0.2s, RectF{ 176, 64, 16, 32 } },
					}
				);
				m_pPlayerUnit = pPlayerUnit;
				m_units.emplace_back(pPlayerUnit);
			}
		}

		// カメラの設定
		{
			m_camera.setScale(1.9);
			m_camera.setTargetScale(1.9);
			m_camera.setCenter(m_pPlayerUnit->getPos());
			m_camera.setTargetCenter(m_pPlayerUnit->getPos());
		}

		if (m_pMapData)
		{
			auto* pMapView = new MapView(m_pMapData.get());
			m_pMapView.reset(pMapView);
		}

		// ポーズ画面は閉じておく
		m_pauseView.setEnable(false);

		m_controllerTexture = TextureAsset(U"controller_switch");
		m_buttonFont = FontAsset(U"font_button");
		m_collectItemSE = AudioAsset(U"sd_collect_item");
		m_unlockDoorSE = AudioAsset(U"sd_unlock_door");
		m_ingameBGM = AudioAsset(U"sd_bgm_ingame");
		m_ingameBGM.setLoop(true);
		m_ingameBGM.setVolume(0.075);
		m_ingameBGM.play();
	}

	GameScene::Impl::~Impl()
	{
	}

	void GameScene::Impl::update()
	{
		if (m_pPlayerUnit)
		{
			m_camera.setTargetCenter(m_pPlayerUnit->getPos());
		}
		m_camera.update();

		switch (m_step)
		{
		case Step::Assign:		stepAssign();		break;
		case Step::Idle:		stepIdle();			break;
		case Step::Move:		stepMove();			break;
		case Step::Pause:		stepPause();		break;
		case Step::RescueAnim:	stepRescueAnim();	break;
		case Step::ReturnAnim:	stepReturnAnim();	break;
		case Step::UseKeyPopup:	stepUseKeyPopup();	break;
		case Step::RescuePopup:	stepRescuePopup();	break;
		case Step::ReturnPopup:	stepReturnPopup();	break;
		case Step::CommonPopup:	stepCommonPopup();	break;
		case Step::GameOver:	stepGameOver();		break;
		case Step::Result:		stepResult();		break;
		case Step::End:			break;
		default:				DEBUG_BREAK(true);	break; // ステップの処理追加忘れ
		}
	}

	void GameScene::Impl::draw() const
	{
		ROUNDRECT_STAGENO_AREA.draw(Palette::Darkslategray).drawFrame(2.0, Palette::Darkgray);
		m_buttonFont(m_stageNoText).drawAt(ROUNDRECT_STAGENO_AREA.center());

		// ポーズボタン
		{
			const auto& buttonRect = m_pauseButton.getRect();
			buttonRect.rounded(3).draw(Palette::Darkolivegreen).drawFrame(1.0, Palette::Black);
			m_buttonFont(U"PAUSE").drawAt(buttonRect.center(), Palette::Black);
		}

		ROUNDRECT_MAPVIEW_AREA.draw(Palette::Darkslategray).drawFrame(2.0, Palette::Darkgray);
		ROUNDRECT_LOG_AREA.draw(Palette::Darkslategray).drawFrame(2.0, Palette::Darkgray);

		// コントローラーの表示
		{
			CIRCLE_CONTROLLER_AREA.draw(Palette::Darkslategray).drawFrame(2.0, Palette::Darkgray);

			const ScopedRenderStates2D samplerState{ SamplerState::ClampNearest };
			const int32 chipSize = 16;
			const int32 xCount = static_cast<int32>(m_controllerTexture.width() / chipSize);
			RectF srcRect{ 0, 0, chipSize, chipSize };
			static const int32 CONTROLLER_CHIP_TABLE[] = { 656, 726, 761, 691 };
			for (size_t i : step(std::size(m_controlButtons)))
			{
				const int32 chipNo = CONTROLLER_CHIP_TABLE[i];
				srcRect.setPos(chipNo % xCount * chipSize, chipNo / xCount * chipSize);
				const auto& centerPos = m_controlButtons[i].getCircle().center;
				m_controllerTexture(srcRect).scaled(5).drawAt(centerPos);
			}
		}
		{
			const ScopedRenderTarget2D target{ m_renderTarget.clear(Palette::Black) };
			const ScopedRenderStates2D blend{ SamplerState::ClampNearest, MakeBlendState() };
			const Transformer2D transformer{ m_camera.createTransformer() };
			if (m_pMapView)
			{
				m_pMapView->draw();
			}

			for (const auto& item : m_items)
			{
				if (item)
				{
					if (item->existOwer())
					{
						continue;
					}
					item->draw();
				}
			}

			for (const auto& unit : m_units)
			{
				if (unit)
				{
					if (not(unit->isEnable()))
					{
						continue;
					}
					unit->draw();
				}
			}
			m_teleportAnim.draw();
		}
		m_renderTarget.rounded(ROUNDRECT_MAPVIEW_AREA.r).drawAt(ROUNDRECT_MAPVIEW_AREA.center());

		// 脱出ボタン
		{
			const auto& buttonRect = m_exitButton.getRect();
			buttonRect.rounded(3).draw(Palette::Darkred).drawFrame(1.0, Palette::Black);
			m_buttonFont(U"脱出").drawAt(buttonRect.center(), Palette::Black);
		}

		// ポーズウィンドウ
		m_pauseView.draw();

		if (m_pMessageBox)
		{
			m_pMessageBox->draw();
		}
	}

	bool GameScene::Impl::isEnd() const
	{
		return (m_step == Step::End);
	}

	SceneKey GameScene::Impl::getNextScene() const
	{
		return m_nextScene;
	}

	void GameScene::Impl::stepAssign()
	{
		m_step = Step::Idle;
	}

	void GameScene::Impl::stepIdle()
	{
		for (auto& unit : m_units)
		{
			if (unit)
			{
				unit->update();
			}
		}

		for (auto& button : m_controlButtons)
		{
			button.update();
		}

		m_pauseButton.update();

		m_exitButton.update();

		if (m_pauseButton.isSelected(Button::Sounds::Select))
		{
			m_pauseView.setEnable(true);
			m_step = Step::Pause;
			return;
		}

		if (m_exitButton.isSelected(Button::Sounds::Select))
		{
			createReturnPopup();
			return;
		}


		if (m_pPlayerUnit)
		{
			const auto& playerPos = MapPosFromGlobalPos(m_pPlayerUnit->getPos());
			for (auto& item : m_items)
			{
				if (item->existOwer())
				{
					continue;
				}

				// 鍵との判定
				if (item->isKey())
				{
					const auto& itemPos = MapPosFromGlobalPos(item->getPos());
					if (itemPos == playerPos)
					{
						item->setOwner(m_pPlayerUnit);
						m_holdKeys.emplace_back(item.get());
						m_collectItemSE.playOneShot();
						break;
					}
				}
			}
			for (auto& pEnemy : m_enemies)
			{
				if (pEnemy)
				{
					if (not(pEnemy->isEnable()))
					{
						continue;
					}
					const auto& enemyPos = MapPosFromGlobalPos(pEnemy->getPos());
					if (playerPos == enemyPos)
					{
						m_step = Step::GameOver;
						return;
					}
				}
			}

			for (auto& targetUnit : m_targetUnits)
			{
				// 救助済み
				if (targetUnit.rescued)
				{
					continue;
				}
				const auto& targetPos = MapPosFromGlobalPos(targetUnit.pTargetUnit->getPos());
				if (targetPos == playerPos)
				{
					m_pRescueUnitTarget = &targetUnit;
					createRescuePopup();
					return;
				}
			}
		}

		// 入れ子パラダイス
		// 後で別にまとめる
		// 通れるか、鍵を使用するかの確認
		{
			static const std::pair<RoomData::Route, RoomData::Route> ROUTE_TABLE[] = {
				{ RoomData::Route::Up, RoomData::Route::Down },
				{ RoomData::Route::Down, RoomData::Route::Up },
				{ RoomData::Route::Left, RoomData::Route::Right },
				{ RoomData::Route::Right, RoomData::Route::Left }
			};
			for (size_t i : step(std::size(m_controlButtons)))
			{
				if (m_controlButtons[i].isSelected(Button::Sounds::Select))
				{
					Point mapPos = MapPosFromGlobalPos(m_pPlayerUnit->getPos());
					auto& nowRoom = m_pMapData->getRoomData(mapPos);
					const auto& route = ROUTE_TABLE[i];
					if (route.first == RoomData::Route::Left)
					{
						m_pPlayerUnit->setMirror(true);
					}
					else if (route.first == RoomData::Route::Right)
					{
						m_pPlayerUnit->setMirror(false);
					}
					if (nowRoom.canPassable(route.first))
					{
						if (nowRoom.isLocked(route.first))
						{
							// 鍵がかかっている
							if (m_holdKeys.size() > 0)
							{
								m_unlockRoomData.emplace(&nowRoom, route.first);
								createUseKeyPopup();
								return;
							}
							else
							{
								createNotHaveKeyPopup();
								return;
							}
						}
						else
						{
							// 移動先
							switch (route.first)
							{
							case bnscup::RoomData::Route::Up:
								mapPos.y -= 1;
								break;
							case bnscup::RoomData::Route::Right:
								mapPos.x += 1;
								break;
							case bnscup::RoomData::Route::Down:
								mapPos.y += 1;
								break;
							case bnscup::RoomData::Route::Left:
								mapPos.x -= 1;
								break;
							default:
								DEBUG_BREAK(true);
								break;
							}
							// 移動先に敵がいないか確認
							for (auto& pEnemy : m_enemies)
							{
								const auto& enemyPos = MapPosFromGlobalPos(pEnemy->getPos());
								if (mapPos == enemyPos)
								{
									const auto& playerMoveDir = route.first;
									const auto& enemyMoveDir = pEnemy->getMoveDirection();
									// 移動方向に対して向かい合っているか
									// ルート用のクラスに分けて計算できると楽
									if (
										   (playerMoveDir == RoomData::Route::Up    and enemyMoveDir == RoomData::Route::Down )
										or (playerMoveDir == RoomData::Route::Right and enemyMoveDir == RoomData::Route::Left )
										or (playerMoveDir == RoomData::Route::Down  and enemyMoveDir == RoomData::Route::Up   )
										or (playerMoveDir == RoomData::Route::Left  and enemyMoveDir == RoomData::Route::Right)
										)
									{
										enemyMove();
										m_step = Step::Move;
										return;
									}
								}
							}
							auto& targetRoom = m_pMapData->getRoomData(mapPos);
							if (targetRoom.canPassable(route.second))
							{
								if (targetRoom.isLocked(route.second))
								{
									// 鍵がかかっている
									if (m_holdKeys.size() > 0)
									{
										m_unlockRoomData.emplace(&targetRoom, route.second);
										createUseKeyPopup();
										return;
									}
									else
									{
										createNotHaveKeyPopup();
										return;
									}
								}
								else
								{
									// 通れる
									m_pPlayerUnit->setTargetPos(MapPosToGlobalPos(mapPos));
									enemyMove();
									m_step = Step::Move;
									return;
								}
							}
						}
					}
				}
			}
		}
		
	}

	void GameScene::Impl::stepMove()
	{
		bool isMoving = false;
		for (auto& unit : m_units)
		{
			if (unit)
			{
				unit->update();
				if (unit->isMoving())
				{
					isMoving = true;
				}
			}
		}
		if (isMoving)
		{
			return;
		}


		for (auto& pEnemy : m_enemies)
		{
			checkEnemyMoveDir(pEnemy);
		}

		m_step = Step::Idle;
	}

	void GameScene::Impl::stepPause()
	{
		m_pauseView.update();

		Step nextStep = Step::Pause;
		if (m_pauseView.isCloseViewButtonSelected())
		{
			// 閉じる
			m_pauseView.setEnable(false);
			nextStep = Step::Idle;
		}
		else if (m_pauseView.isReturnStageSelectButtonSelected())
		{
			m_nextScene = SceneKey::StageSelect;
			nextStep = Step::End;
		}
		else if (m_pauseView.isReturnTitleButtonSelected())
		{
			m_nextScene = SceneKey::Title;
			nextStep = Step::End;
		}
		m_step = nextStep;
	}

	void GameScene::Impl::stepRescueAnim()
	{
		m_teleportAnim.update();
		if (not(m_teleportAnim.isEnd()))
		{
			return;
		}
		m_teleportAnim.reset();
		for (const auto& targetUnit : m_targetUnits)
		{
			if (not(targetUnit.rescued))
			{
				m_step = Step::Idle;
				return;
			}
		}
		createReturnPopup();
	}

	void GameScene::Impl::stepReturnAnim()
	{
		m_teleportAnim.update();
		if (not(m_teleportAnim.isEnd()))
		{
			return;
		}
		m_teleportAnim.reset();
		m_step = Step::Result;
	}

	void GameScene::Impl::stepUseKeyPopup()
	{
		if (m_pMessageBox.get() == nullptr)
		{
			DEBUG_BREAK(true); // 処理しようがない。
			m_step = Step::Idle;
			return;
		}

		m_pMessageBox->update();

		if (m_pMessageBox->isYesSelected())
		{
			// アンロック
			if (m_unlockRoomData)
			{
				m_unlockRoomData->pTargetRoom->unlock(m_unlockRoomData->unlockRoute);
				m_unlockDoorSE.playOneShot();
			}
		}
		else if (m_pMessageBox->isNoSelected() or m_pMessageBox->isExitCrossSelected())
		{
			// メッセージキャンセル
		}
		else
		{
			return;
		}

		// 処理終わり
		m_unlockRoomData.reset();
		m_pMessageBox.reset();
		m_step = Step::Idle;
	}

	void GameScene::Impl::stepRescuePopup()
	{
		if (m_pMessageBox.get() == nullptr)
		{
			DEBUG_BREAK(true); // 処理しようがない。
			m_pRescueUnitTarget = nullptr;
			m_step = Step::Idle;
			return;
		}

		m_pMessageBox->update();

		Step nextStep = Step::Idle;
		if (m_pMessageBox->isYesSelected())
		{
			m_pRescueUnitTarget->rescued = Rescued::Yes;
			m_pRescueUnitTarget->pTargetUnit->setEnable(false);
			m_teleportAnim.setPos(m_pRescueUnitTarget->pTargetUnit->getPos());
			m_teleportAnim.reset();
			m_teleportAnim.setEnable(true);
			nextStep = Step::RescueAnim;
		}
		else if (m_pMessageBox->isNoSelected() or m_pMessageBox->isExitCrossSelected())
		{
			// メッセージキャンセル
		}
		else
		{
			return;
		}

		// 処理終わり
		m_pRescueUnitTarget = nullptr;
		m_pMessageBox.reset();
		m_step = nextStep;
	}

	void GameScene::Impl::stepReturnPopup()
	{
		if (m_pMessageBox.get() == nullptr
			or m_pPlayerUnit == nullptr)
		{
			DEBUG_BREAK(true); // 処理しようがない。
			m_step = Step::Idle;
			return;
		}

		m_pMessageBox->update();

		Step nextStep = Step::Idle;
		if (m_pMessageBox->isYesSelected())
		{
			m_teleportAnim.setPos(m_pPlayerUnit->getPos());
			m_teleportAnim.reset();
			m_teleportAnim.setEnable(true);
			m_pPlayerUnit->setEnable(false);
			m_pPlayerUnit = nullptr;
			nextStep = Step::ReturnAnim;
		}
		else if (m_pMessageBox->isNoSelected() or m_pMessageBox->isExitCrossSelected())
		{
			// メッセージキャンセル
		}
		else
		{
			return;
		}

		// 処理終わり
		m_pMessageBox.reset();
		m_step = nextStep;
	}

	void GameScene::Impl::stepCommonPopup()
	{
		if (m_pMessageBox.get() == nullptr)
		{
			DEBUG_BREAK(true);
			return;
		}

		m_pMessageBox->update();

		if (m_pMessageBox->isOKSelected())
		{
			m_pMessageBox.reset();
			m_step = Step::Idle;
			return;
		}
	}

	void GameScene::Impl::stepGameOver()
	{
		m_nextScene = SceneKey::StageSelect;
		m_step = Step::End;
		return;
	}

	void GameScene::Impl::stepResult()
	{
		m_nextScene = SceneKey::StageSelect;
		m_step = Step::End;
		return;
	}

	void GameScene::Impl::createUseKeyPopup()
	{
		auto* pMessageBox = new MessageBox(MessageBox::ButtonStyle::YesNo, MessageBox::ExistCrossButton::No, U"鍵を使用しますか？");
		m_pMessageBox.reset(pMessageBox);
		m_step = Step::UseKeyPopup;
	}

	void GameScene::Impl::createRescuePopup()
	{
		String message =
			U"救助対象を見つけました！\n"
			U"転送します。";
		auto* pMessageBox = new MessageBox(MessageBox::ButtonStyle::OnlyOK, MessageBox::ExistCrossButton::No, message);
		m_pMessageBox.reset(pMessageBox);
		m_step = Step::RescuePopup;
	}

	void GameScene::Impl::createReturnPopup()
	{
		String exitMessage = U"";
		size_t rescueCount = 0;
		for (const auto& targetUnit : m_targetUnits)
		{
			if (targetUnit.rescued)
			{
				rescueCount++;
			}
		}
		MessageBox* pMessageBox = nullptr;
		if (rescueCount != m_targetUnits.size())
		{
			exitMessage =
				U"まだ助けていないユニットがいます。\n"
				U"脱出しますか？";
			pMessageBox = new MessageBox(MessageBox::ButtonStyle::YesNo, MessageBox::ExistCrossButton::No, exitMessage);
		}
		else
		{
			exitMessage =
				U"すべてのユニットを救助しました！\n"
				U"脱出します。";
			pMessageBox = new MessageBox(MessageBox::ButtonStyle::OnlyOK, MessageBox::ExistCrossButton::No, exitMessage);
		}
		DEBUG_BREAK(pMessageBox == nullptr);
		m_pMessageBox.reset(pMessageBox);
		m_step = Step::ReturnPopup;
	}

	void GameScene::Impl::createNotHaveKeyPopup()
	{
		String message =
			U"鍵がかかっています。\n"
			U"どこかに落ちている鍵を探しましょう！";
		auto* pMessageBox = new MessageBox(MessageBox::ButtonStyle::OnlyOK, MessageBox::ExistCrossButton::No, message);
		m_pMessageBox.reset(pMessageBox);
		m_step = Step::CommonPopup;
	}

	void GameScene::Impl::enemyMove()
	{
		for (auto& pEnemy : m_enemies)
		{
			if (pEnemy == nullptr)
			{
				continue;
			}
			checkEnemyMoveDir(pEnemy);
			Point enemyPos = MapPosFromGlobalPos(pEnemy->getPos());
			switch (pEnemy->getMoveDirection())
			{
			case RoomData::Route::Up: enemyPos.y -= 1; break;
			case RoomData::Route::Right: enemyPos.x += 1; break;
			case RoomData::Route::Down: enemyPos.y += 1; break;
			case RoomData::Route::Left: enemyPos.x -= 1; break;
			default: DEBUG_BREAK(true); break;
			}
			pEnemy->setTargetPos(MapPosToGlobalPos(enemyPos));
		}
	}

	void GameScene::Impl::checkEnemyMoveDir(Enemy* pEnemy)
	{
		if (pEnemy == nullptr)
		{
			return;
		}
		Point enemyPos = MapPosFromGlobalPos(pEnemy->getPos());
		const auto& roomData = m_pMapData->getRoomData(enemyPos);
		const auto& moveDirection = pEnemy->getMoveDirection();
		if (not(roomData.canPassable(moveDirection))
			|| roomData.isLocked(moveDirection))
		{
			if (pEnemy->getMoveType() == Enemy::MoveType::UpDown)
			{
				if (moveDirection == RoomData::Route::Up)
				{
					pEnemy->setMoveDirection(RoomData::Route::Down);
				}
				else
				{
					pEnemy->setMoveDirection(RoomData::Route::Up);
				}
			}
			else if (pEnemy->getMoveType() == Enemy::MoveType::LeftRight)
			{
				if (moveDirection == RoomData::Route::Left)
				{
					pEnemy->setMoveDirection(RoomData::Route::Right);
					pEnemy->setMirror(false);
				}
				else
				{
					pEnemy->setMoveDirection(RoomData::Route::Left);
					pEnemy->setMirror(true);
				}
			}
			DEBUG_BREAK(not(roomData.canPassable(pEnemy->getMoveDirection())));
			DEBUG_BREAK(roomData.isLocked(pEnemy->getMoveDirection()));
		}
	}
	
	//==================================================

	GameScene::GameScene(const super::InitData& init)
		: super{ init }
		, m_pImpl{ nullptr }
	{
		m_pImpl.reset(new Impl(getData().stageNo));
	}

	GameScene::~GameScene()
	{
	}

	void GameScene::update()
	{
		if (m_pImpl)
		{
			m_pImpl->update();
			if (m_pImpl->isEnd())
			{
				auto& sceneData = getData();
				sceneData.nextScene = m_pImpl->getNextScene();
				changeScene(SceneKey::Load, 1.0s);
			}
		}
	}

	void GameScene::draw() const
	{
		if (m_pImpl)
		{
			m_pImpl->draw();
		}
	}


}
