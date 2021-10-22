 #include "External/HandmadeMath.h"
#define STB_IMAGE_IMPLEMENTATION
#include "External/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "Blind_Render.cpp"

#include <vector>

f32 RandomFloat(f32, f32);

struct iv2 {
	s32 X, Y;
};

const u32 EFLAG_Controlled = 0x1;
const u32 EFLAG_RenderRect = 0x2;
const u32 EFLAG_SimMovement = 0x4;
const u32 EFLAG_CollideWithMap = 0x8;
const u32 EFLAG_DoGravity = 0x10;
const u32 EFLAG_Solid = 0x20;
const u32 EFLAG_WinLevel = 0x40;
const u32 EFLAG_ScriptedPath = 0x80;
const u32 EFLAG_RenderRectWhenWon = 0x100; // @TODO Add in processing for this flag

struct scripted_path_node {
	hmm_v2 Destination;
	f32 OverTime;
};

struct entity {
	s64 Flags;

	// Location
	hmm_v3 Position;
	b8 Grounded;
	// Seperate from Grounded, so that jump on downward slopes are more likely to happen.
	b8 CanJump;

	// Size
	hmm_v2 Dimentions;

	// Movement
	hmm_v2 Velocity;
	hmm_v2 Acceleration;

	scripted_path_node *ScriptedPath;
	u32 ScriptedPathLength;
	u32 CurrentNode;
	f32 ElapsedTime;
	b8 RepeatPath;

	// Visual
	// @TODO(Michael) replace/add information about what texture this should use. Likely will be 4 texture coords and 1 index into our texture array?
	hmm_v3 Color;
};

global_var u32 EntityListCount = 0;
global_var entity EntityList[100] = {};

void AppendEntity(entity Data) {
	EntityList[EntityListCount] = Data;
	EntityListCount++;
}

#include "Blind_Map.cpp"

const u32 PFLAG_DoLifeTime = 0x1;
const u32 PFLAG_CollideWithMap = 0x2;
const u32 PFLAG_RenderRect = 0x4;
const u32 PFLAG_SimMovement = 0x8;

struct partical {
	s64 Flags;
	hmm_v3 Position;
	
	hmm_v2 Velocity;

	hmm_v2 Dimentions;

	hmm_v3 Color;

	float LifeTime;
	float CurTime;
};

struct controller_state {
	// @Compression We could pack these fields together; They're all 1-bit booleans.
	b8 Up;
	b8 Down;
	b8 Left;
	b8 Right;
    
	b8 A;
	b8 B;
	b8 X;
	b8 Y;
    
	b8 L;
	b8 L2;
	b8 L3;
    
	b8 R;
	b8 R2;
	b8 R3;
    
	b8 Start;
	b8 Select;
};

struct mouse_state {
	iv2 Position;
	b8 LeftClick;
	b8 RightClick;
};

struct input_state {
	controller_state Current;
	controller_state Prevous;
	mouse_state Mouse;
};

const u32 ParticalMax = 5000;
global_var std::vector<partical> ParticalList;

struct game_state {
	s32 CurrentLevel;
	s32 *TileMap[MapHeight][MapWidth];
	b8 CanDraw;
	b8 LevelWon;
	b8 Init;
	u32 PlayerEntity;
	
} GameState;

/**
 * Checks the tilemap for colision, displaces the entity out of solid tiles if nessary.
 *
 * @param start point describing the start of the stright line sensor
 * @param len   How long the check is.
 * @param dir   What direction the check is in as a unit vector
 * @param tiles the tilemap to check colision aginst
 * @return the distance between the first solid pixel
 *         and the end of the line described by start, len, and dir.
 */
s32 MapColisionCheck (iv2 Start, s32 Length, iv2 Direction, s32* TileMap[][MapWidth]) {
	for (int Index = 1; Index <= Length; Index++) {
		iv2 Position = {(Start.X + Index * Direction.X), (Start.Y + Index * Direction.Y)};
		iv2 Tile;

		if (Position.X >= 0) {
			Tile.X = Position.X / 32;
		}
		else {
			Tile.X = 1 - Position.X / 32;
		}
		
		if (Position.Y >= 0) {
			Tile.Y = 29 - Position.Y / 32;
		}
		else {
			Tile.Y = 30 - Position.Y / 32;
		}
		
		iv2 SubTile = {Position.X % 32, Position.Y % 32};
		
		if (Tile.X >= 0 &&
		    Tile.Y >= 0 &&
		    Tile.X < MapWidth &&
		    Tile.Y < MapHeight) {
			u32 Height = TileMap[Tile.Y][Tile.X][SubTile.X];
			if (SubTile.Y < Height) {
				return Length - Index;
			}
		}
		else {
			return Length - Index;
		}
	}
	return 0;
}

b8 SinglePointCollisionCheck(iv2 Position, u32* TileMap[][MapWidth]) {
	iv2 Tile = {Position.X / 32, 29 - Position.Y / 32};
	iv2 SubTile = {Position.X % 32, Position.Y % 32};
	if (Tile.X >= 0 &&
	    Tile.Y >= 0 &&
	    Tile.X < MapWidth &&
	    Tile.Y < MapHeight) {
		u32 Height = TileMap[Tile.Y][Tile.X][SubTile.X];
		if (SubTile.Y < Height) {
			return true;
		}
	}
	return false;
}

s32 EntityCollisionCheck(iv2 Start, s32 Length, iv2 Direction, entity Entity) {
	s32 Top = Entity.Position.Y + Entity.Dimentions.Y/2;
	s32 Bottom = Entity.Position.Y - Entity.Dimentions.Y/2;
	
	s32 Left = Entity.Position.X - Entity.Dimentions.X/2;
	s32 Right = Entity.Position.X + Entity.Dimentions.X/2;
	
	for (int Index = 1; Index <= Length; Index++) {
		iv2 Position = {(Start.X + Index * Direction.X), (Start.Y + Index * Direction.Y)};
		
		if (Position.X > Left && Position.X < Right &&
		    Position.Y > Bottom && Position.Y < Top) {
			return Length - Index;
		}
		
	}
	return 0;	
}

b8 IsInsideEntity(iv2 Position, entity Entity) {
	s32 Top = Entity.Position.Y + Entity.Dimentions.Y/2;
	s32 Bottom = Entity.Position.Y - Entity.Dimentions.Y/2;
	
	s32 Left = Entity.Position.X - Entity.Dimentions.X/2;
	s32 Right = Entity.Position.X + Entity.Dimentions.X/2;
	
	if (Position.X > Left && Position.X < Right &&
	    Position.Y > Bottom && Position.Y < Top) {
		return true;
	}

	return false;
}

void SpawnPartical(hmm_v2 SpawnPoint, f32 LifeTime) {
	partical Partical = {};
	Partical.Flags = PFLAG_DoLifeTime | PFLAG_RenderRect | PFLAG_SimMovement;
	Partical.Position.XY = SpawnPoint;
	Partical.Position.Z = 4;
	Partical.Velocity = hmm_v2{RandomFloat(-10, 10), RandomFloat(-10, 10)};
	Partical.Dimentions = hmm_v2{2, 2};
	Partical.Color = hmm_v3{0, RandomFloat(0.6, 1.0), RandomFloat(0.0, 0.6)};
	Partical.LifeTime = LifeTime;
	Partical.CurTime = 0;
	
	ParticalList.push_back(Partical);
}

void SpawnParticalAroundTileMap(iv2 StartPoint, iv2 Direction, s32 Length, f32 LifeTime) {
	s32 Offset = (Length - 1) - MapColisionCheck(StartPoint, Length, Direction, GameState.TileMap);
	
	if (Offset != (Length - 1)) {
		hmm_v2 fOffset = {(f32)(Offset * Direction.X), (f32)(Offset * Direction.Y)};
		hmm_v2 fStartPoint = {(f32)StartPoint.X, (f32)StartPoint.Y};
		SpawnPartical(fStartPoint + fOffset, LifeTime);
	}
}

void SpawnParticalAroundEntity(iv2 StartPoint, iv2 Direction, s32 Length, entity Entity, f32 LifeTime) {
	s32 Offset = (Length - 1) - EntityCollisionCheck(StartPoint, Length, Direction, Entity);
	
	if (Offset != (Length - 1)) {
		hmm_v2 fOffset = {(f32)(Offset * Direction.X), (f32)(Offset * Direction.Y)};
		hmm_v2 fStartPoint = {(f32)StartPoint.X, (f32)StartPoint.Y};
		SpawnPartical(fStartPoint + fOffset, LifeTime);
	}
}

void BlindSimulateAndRender(f32 DeltaTime, input_state InputState) {
	
	if (InputState.Current.A && GameState.LevelWon) {
		GameState.Init = false;
		GameState.LevelWon = false;
		GameState.CurrentLevel++;
	}
		
	
	if (!GameState.Init) {
		GameState.Init = true;
		GameState.CanDraw = false;
		GameState.LevelWon = false;

		for (s32 XIndex = 0; XIndex < WindowWidth; XIndex++) {
			for (s32 YIndex = 0; YIndex < WindowHeight; YIndex++) {
				u8 *Pixel = &UserDrawTextureData[((XIndex) + (YIndex) * (WindowWidth)) * 4];
				*(u32*)Pixel = 0x00000000;
			}
		}
		Render_UpdateTextureArray(4, UserDrawTextureData, WindowWidth * 4);
		
		memset(EntityList, 0, sizeof(EntityList));
		EntityListCount = 0; 

		entity StagingEntity = {};
		
		StagingEntity.Flags |= EFLAG_Controlled | EFLAG_DoGravity | EFLAG_SimMovement | EFLAG_CollideWithMap;
		//StagingEntity.Flags |= EFLAG_RenderRect; // For Debug!
		StagingEntity.Position.XY = playerSpawn[GameState.CurrentLevel];
		StagingEntity.Position.Z = 5;
		StagingEntity.Dimentions = {24, 32};
		StagingEntity.Color = {1.0, 0.5, 0.0};
		GameState.PlayerEntity = EntityListCount;
		AppendEntity(StagingEntity);

		StagingEntity = {};
		StagingEntity.Flags |= EFLAG_RenderRect | EFLAG_WinLevel;
		StagingEntity.Position.XY = winArea[GameState.CurrentLevel];
		StagingEntity.Position.Z = 6;
		StagingEntity.Dimentions = {32 * 4, 32 * 3};
		StagingEntity.Color = {0.0, 1.0, 0.0};
		AppendEntity(StagingEntity);

		switch (GameState.CurrentLevel) {
		case 0:
			memcpy(GameState.TileMap, LEVEL_01_MAP, sizeof(LEVEL_01_MAP));
			break;
		case 1:
			memcpy(GameState.TileMap, LEVEL_02_MAP, sizeof(LEVEL_02_MAP));
			break;
		case 2:
			memcpy(GameState.TileMap, LEVEL_03_MAP, sizeof(LEVEL_03_MAP));
			break;
		case 3:
			memcpy(GameState.TileMap, LEVEL_04_MAP, sizeof(LEVEL_04_MAP));
			break;
		case 4:
			memcpy(GameState.TileMap, LEVEL_05_MAP, sizeof(LEVEL_05_MAP));
			break;
		case 5:
			memcpy(GameState.TileMap, LEVEL_06_MAP, sizeof(LEVEL_06_MAP));
			break;
		case 6:
			memcpy(GameState.TileMap, LEVEL_07_MAP, sizeof(LEVEL_07_MAP));
			break;
		case 7:
			memcpy(GameState.TileMap, LEVEL_08_MAP, sizeof(LEVEL_08_MAP));
			break;
		case 8:
			memcpy(GameState.TileMap, LEVEL_09_MAP, sizeof(LEVEL_09_MAP));
			break;
		}

		InitMap(GameState.TileMap, GameState.CurrentLevel);

		GameState.CanDraw = true;

		ParticalList.reserve(ParticalMax);
	}
	
	// Level foreground and background
	// @TODO(Michael) this shouldn't have to be here, but it must until we get the depth buffer working.
	if (GameState.LevelWon) {
		Render_EnqueueQuadSample(
	  	{
				hmm_v3{(f32)WindowWidth/2.f, (f32)WindowHeight/2.f, 10},
				hmm_v2{(f32)WindowWidth, (f32)WindowHeight},
				1,
				hmm_v2{0.0, 0.0},
				hmm_v2{1.0, 0.0},
				hmm_v2{0.0, 1.0},
				hmm_v2{1.0, 1.0},
		  });
	}

	
	if (InputState.Current.X) {
		// Debug TileMap	
		Render_EnqueueQuadSample(
	  	{
				hmm_v3{(f32)WindowWidth/2.f, (f32)WindowHeight/2.f, 0},
				hmm_v2{(f32)WindowWidth, (f32)WindowHeight},
				0,
				hmm_v2{0.0, 0.0},
				hmm_v2{1.0, 0.0},
				hmm_v2{0.0, 1.0},
				hmm_v2{1.0, 1.0},
	  	});
	}
	
	
	Render_EnqueueQuadSample(
	  {
			hmm_v3{(f32)WindowWidth/2.f, (f32)WindowHeight/2.f, 0},
			hmm_v2{(f32)WindowWidth, (f32)WindowHeight},
			4,
			hmm_v2{0.0, 0.0},
			hmm_v2{1.0, 0.0},
			hmm_v2{0.0, 1.0},
			hmm_v2{1.0, 1.0},
	 });

	// @TODO(Michael) @Jank This is totally jank, in reality we need to fill in a line between each sample. We could probably achieve this by rendering lines to the UserDraw texture and then drawing that texture to the screen.
	if (GameState.CanDraw) {
		if (InputState.Mouse.LeftClick) {
			
			for (s32 XIndex = InputState.Mouse.Position.X - 5;
			     XIndex < InputState.Mouse.Position.X + 5;
			     XIndex++) {
				for (s32 YIndex = InputState.Mouse.Position.Y - 5;
				     YIndex < InputState.Mouse.Position.Y + 5;
				     YIndex++) {
					
					if (XIndex >= 0 && XIndex < WindowWidth &&
					    YIndex >= 0 && YIndex < WindowHeight) {
						u8 *Pixel = &UserDrawTextureData[((XIndex) + (YIndex) * (WindowWidth)) * 4];
						*(u32*)Pixel = 0xFF186818;
					}
					
				}
			}
			
		}

		if (InputState.Mouse.RightClick) {
			
			for (s32 XIndex = InputState.Mouse.Position.X - 20;
			     XIndex < InputState.Mouse.Position.X + 5;
			     XIndex++) {
				for (s32 YIndex = InputState.Mouse.Position.Y - 20;
				     YIndex < InputState.Mouse.Position.Y + 5;
				     YIndex++) {
					
					if (XIndex >= 0 && XIndex < WindowWidth &&
					    YIndex >= 0 && YIndex < WindowHeight) {
						u8 *Pixel = &UserDrawTextureData[((XIndex) + (YIndex) * (WindowWidth)) * 4];
						*(u32*)Pixel = 0x00000000;
					}
					
				}
			}
			
		}
		
		Render_UpdateTextureArray(4, UserDrawTextureData, WindowWidth * 4);
	}
	
	for (s32 Index = 0; Index < ArraySize(EntityList); Index++) {
		entity * const Entity = &EntityList[Index];
	
		if (Entity->Flags & EFLAG_DoGravity) {
			if (!Entity->Grounded) {
				Entity->Acceleration.Y = -600;
			}
			else {
				Entity->Acceleration.Y = 0;
			}
		}

		if (Entity->Flags & EFLAG_ScriptedPath) {
			if (Entity->ElapsedTime > Entity->ScriptedPath[Entity->CurrentNode].OverTime) {
				Entity->Position.XY = Entity->ScriptedPath[Entity->CurrentNode].Destination;
				Entity->ElapsedTime = 0;

				if (Entity->CurrentNode < Entity->ScriptedPathLength - 1) {
					Entity->CurrentNode++;
				}
				else if (Entity->RepeatPath) {
					Entity->CurrentNode = 0;
				}
				else {
					Entity->Flags &= ~EFLAG_ScriptedPath;
					goto ExitScriptedPath; // @TODO(Michael) GOTO is not the worst thing, but control flow can probably be made clearer here.
				}
			}

			if (Entity->ElapsedTime == 0) {
				hmm_v2 Displacement = Entity->ScriptedPath[Entity->CurrentNode].Destination - Entity->Position.XY;
				Entity->Velocity = Displacement / Entity->ScriptedPath[Entity->CurrentNode].OverTime;
			}
			
			Entity->ElapsedTime += DeltaTime;
		ExitScriptedPath:;
		}
	
		if (Entity->Flags & EFLAG_SimMovement) {
			// classic kinematic eqs
			// pos += Velocity * delta + acl * delta * delta * 0.5
			Entity->Position.XY += (Entity->Velocity * DeltaTime) + (Entity->Acceleration * DeltaTime * DeltaTime * 0.5);
			// vel += acl * delta
			Entity->Velocity += Entity->Acceleration * DeltaTime;
		}
		
		if (Entity->Flags & EFLAG_Controlled) {
			if (InputState.Current.Right) {
				Entity->Velocity.X = 120;
			}
			else if (InputState.Current.Left) {
				Entity->Velocity.X = -120;
			}
			else {
				Entity->Velocity.X = 0;
			}
		}

		if (Entity->Flags & EFLAG_CollideWithMap) {
			// Now lets correct movement that has placed objects out of bounds.
			Entity->CanJump = false;
			Entity->Grounded = false; // assume that we are in the air. set otherwise if our legs collide with anything.
		
			// Torso: push us out of walls
			{ // Left Torso
				int disp =
					MapColisionCheck (
					                  {(int)(Entity->Position.X),
					                   (int)(Entity->Position.Y)},
					                  (int)(Entity->Dimentions.X / 2),
					                  {-1, 0},
					                  GameState.TileMap);
				if (disp) {
					Entity->Position.X += disp;
					Entity->Velocity.X = 0;

					for (s32 Count = 0; Count < 6; Count++) {
						iv2 CheckPos = {
							(s32)(Entity->Position.X - Entity->Dimentions.X/2),
							(s32)RandomFloat(Entity->Position.Y - Entity->Dimentions.Y/2, Entity->Position.Y + Entity->Dimentions.Y/2)
						};
						SpawnParticalAroundTileMap(CheckPos, {-1, 0}, 4, 0.32);
					}
				}
			}
		
			{ // Right Torso
				int disp =
					MapColisionCheck (
					                  {(int)(Entity->Position.X),
					                   (int)(Entity->Position.Y)},
					                  (int)(Entity->Dimentions.X / 2),
					                  {1, 0},
					                  GameState.TileMap);
				if (disp) {
					Entity->Position.X -= disp;
					Entity->Velocity.X = 0;

					for (s32 Count = 0; Count < 6; Count++) {
						iv2 CheckPos = {
							(s32)(Entity->Position.X + Entity->Dimentions.X/2),
							(s32)RandomFloat(Entity->Position.Y - Entity->Dimentions.Y/2, Entity->Position.Y + Entity->Dimentions.Y/2)
						};
						SpawnParticalAroundTileMap(CheckPos, {1, 0}, 4, 0.32);
					}
				}
			}
		
			//Legs
			if (Entity->Velocity.Y <= 0) { //we need to "push up" when we are falling.
				int deltaYr =
					MapColisionCheck (
					                  {(int)(Entity->Position.X + Entity->Dimentions.X/2 - 2),
					                   (int)(Entity->Position.Y)},
					                  (int)(Entity->Dimentions.Y/2 + 1),
					                  {0, -1},
					                  GameState.TileMap);
				
				int deltaYl =
					MapColisionCheck (
					                  {(int)(Entity->Position.X - Entity->Dimentions.X/2 + 2),
					                   (int)(Entity->Position.Y)},
					                  (int)(Entity->Dimentions.Y/2 + 1),
					                  {0, -1},
					                  GameState.TileMap);
				
				if (deltaYr == 0 && deltaYl == 0) {
				}
				else {
					Entity->Velocity.Y = 0;
					Entity->Grounded = true;
					
					if (deltaYr > deltaYl)
						Entity->Position.Y += deltaYr - 1;
					else
						Entity->Position.Y += deltaYl - 1;

					for (s32 Count = 0; Count < 6; Count++) {
						iv2 CheckPos = {
							(s32)RandomFloat(Entity->Position.X - Entity->Dimentions.X/2, Entity->Position.X + Entity->Dimentions.X/2),
							(s32)(Entity->Position.Y - Entity->Dimentions.Y/2)
						};
						SpawnParticalAroundTileMap(CheckPos, {0, -1}, 8, 0.10);
					}
					
				}
			}
			else { //if we're rising, we should instead push us out of walls
				{ // Left Leg
					int disp =
						MapColisionCheck(
						                 {(int)(Entity->Position.X),
						                  (int)(Entity->Position.Y - Entity->Dimentions.Y/2 + 1)},
						                 (int)(Entity->Dimentions.X / 2),
						                 {-1, 0},
						                 GameState.TileMap);
					if (disp) {
						Entity->Position.X += disp;
						Entity->Velocity.X = 0;
					}
				}
			
				{ // Right Leg
					int disp =
						MapColisionCheck (
						                  {(int)(Entity->Position.X),
						                   (int)(Entity->Position.Y - Entity->Dimentions.Y/2 + 1)},
						                  (int)(Entity->Dimentions.X / 2),
						                  {1, 0},
						                  GameState.TileMap);
					if (disp) {
						Entity->Position.X -= disp;
						Entity->Velocity.X = 0;
					}
				}
			}

			// Can Jump
			if (Entity->Velocity.Y <= 0) {
				int DeltaYr =
					MapColisionCheck (
					                  {(int)(Entity->Position.X + Entity->Dimentions.X/2 - 2),
					                   (int)(Entity->Position.Y)},
					                  (int)(Entity->Dimentions.Y/2 + 12),
					                  {0, -1},
					                  GameState.TileMap);
				
				int DeltaYl =
					MapColisionCheck (
					                  {(int)(Entity->Position.X - Entity->Dimentions.X/2 + 2),
					                   (int)(Entity->Position.Y)},
					                  (int)(Entity->Dimentions.Y/2 + 12),
					                  {0, -1},
					                  GameState.TileMap);

				if (DeltaYl != 0 || DeltaYr != 0) {
					Entity->CanJump = true;
				}
			}
		
			//Head
			if (Entity->Velocity.Y >= 0) { // if we're rising, lets bonk our head.
				int deltaYr =
					MapColisionCheck(
					                 {(int)(Entity->Position.X + Entity->Dimentions.X/2 - 2),
					                  (int)(Entity->Position.Y)},
					                 (int)(Entity->Dimentions.Y/2 + 1),
					                 {0, 1},
					                 GameState.TileMap);
				int deltaYl =
					MapColisionCheck(
					                 {(int)(Entity->Position.X - Entity->Dimentions.X/2 + 2),
					                  (int)(Entity->Position.Y)},
					                 (int)(Entity->Dimentions.Y/2 + 1),
					                 {0, 1},
					                 GameState.TileMap);
			
				if (deltaYr != 0 || deltaYl != 0) {
					Entity->Velocity.Y = 0;
				
					if (deltaYr > deltaYl)
						Entity->Position.Y -= deltaYr;
					else
						Entity->Position.Y -= deltaYl;

					for (s32 Count = 0; Count < 40; Count++) {
						iv2 CheckPos = {
							(s32)RandomFloat(Entity->Position.X - Entity->Dimentions.X/2, Entity->Position.X + Entity->Dimentions.X/2),
							(s32)(Entity->Position.Y + Entity->Dimentions.Y/2)};
						SpawnParticalAroundTileMap(CheckPos, {0, 1}, 3, 0.32);
					}
				}
			}
			else { //if we're falling, lets push us out of walls.
				{ // Left Head
					int disp =
						MapColisionCheck(
						                 {(int)(Entity->Position.X),
						                  (int)(Entity->Position.Y + Entity->Dimentions.Y/2 - 1)},
						                 (int)(Entity->Dimentions.X / 2),
						                 {-1, 0},
						                 GameState.TileMap);
					if (disp) {
						Entity->Position.X += disp;
						Entity->Velocity.X = 0;
					}
				}
			
				{ // Right Head
					int disp =
						MapColisionCheck(
						                 {(int)(Entity->Position.X),
						                  (int)(Entity->Position.Y + Entity->Dimentions.Y/2 - 1)},
						                 (int)(Entity->Dimentions.X / 2),
						                 {1, 0},
						                 GameState.TileMap);
					if (disp) {
						Entity->Position.X -= disp;
						Entity->Velocity.X = 0;
					}
				}
			}

			// Entity Collision
			for (s32 CollisionIndex = 0; CollisionIndex  < ArraySize(EntityList); CollisionIndex++) {
				entity * const CollisionEntity = &EntityList[CollisionIndex];
				if (CollisionEntity->Flags & EFLAG_Solid) {
					// Torso: push us out of walls
					{ // Left Torso
						int disp =
							EntityCollisionCheck (
							                  {(int)(Entity->Position.X),
							                   (int)(Entity->Position.Y)},
							                  (int)(Entity->Dimentions.X / 2),
							                  {-1, 0},
							                  *CollisionEntity);
						if (disp) {
							Entity->Position.X += disp;
							Entity->Velocity.X = 0;

							for (s32 Count = 0; Count < 6; Count++) {
								iv2 CheckPos = {
									(s32)(Entity->Position.X - Entity->Dimentions.X/2),
									(s32)RandomFloat(Entity->Position.Y - Entity->Dimentions.Y/2, Entity->Position.Y + Entity->Dimentions.Y/2)
								};
								SpawnParticalAroundEntity(CheckPos, {-1, 0}, 4, *CollisionEntity, 0.32);
							}
						}
					}
		
					{ // Right Torso
						int disp =
							EntityCollisionCheck (
							                  {(int)(Entity->Position.X),
							                   (int)(Entity->Position.Y)},
							                  (int)(Entity->Dimentions.X / 2),
							                  {1, 0},
							                  *CollisionEntity);
						if (disp) {
							Entity->Position.X -= disp;
							Entity->Velocity.X = 0;

							for (s32 Count = 0; Count < 6; Count++) {
								iv2 CheckPos = {
									(s32)(Entity->Position.X + Entity->Dimentions.X/2),
									(s32)RandomFloat(Entity->Position.Y - Entity->Dimentions.Y/2, Entity->Position.Y + Entity->Dimentions.Y/2)
								};
								SpawnParticalAroundEntity(CheckPos, {1, 0}, 4, *CollisionEntity, 0.32);
							}
						}
					}
		
					//Legs
					if (Entity->Velocity.Y <= 0) { //we need to "push up" when we are falling.
						int deltaYr =
							EntityCollisionCheck (
							                  {(int)(Entity->Position.X + Entity->Dimentions.X/2 - 2),
							                   (int)(Entity->Position.Y)},
							                  (int)(Entity->Dimentions.Y/2 + 1),
							                  {0, -1},
							                  *CollisionEntity);
				
						int deltaYl =
							EntityCollisionCheck (
							                  {(int)(Entity->Position.X - Entity->Dimentions.X/2 + 2),
							                   (int)(Entity->Position.Y)},
							                  (int)(Entity->Dimentions.Y/2 + 1),
							                  {0, -1},
							                  *CollisionEntity);
				
						if (deltaYr == 0 && deltaYl == 0) {
							Entity->Grounded = false;
						}
						else {
							Entity->Velocity.Y = 0;
							Entity->Grounded = true;
					
							if (deltaYr > deltaYl)
								Entity->Position.Y += deltaYr - 1;
							else
								Entity->Position.Y += deltaYl - 1;

							for (s32 Count = 0; Count < 6; Count++) {
								iv2 CheckPos = {
									(s32)RandomFloat(Entity->Position.X - Entity->Dimentions.X/2, Entity->Position.X + Entity->Dimentions.X/2),
									(s32)(Entity->Position.Y - Entity->Dimentions.Y/2)
								};
								SpawnParticalAroundEntity(CheckPos, {0, -1}, 8, *CollisionEntity, 0.10);
							}

							Entity->Velocity.X += CollisionEntity->Velocity.X;
							if (CollisionEntity->Velocity.Y < 0) {
								Entity->Velocity.Y += CollisionEntity->Velocity.Y;
							}
						}
					}
					else { //if we're rising, we should instead push us out of walls
						{ // Left Leg
							int disp =
								EntityCollisionCheck(
								                 {(int)(Entity->Position.X),
								                  (int)(Entity->Position.Y - Entity->Dimentions.Y/2 + 1)},
								                 (int)(Entity->Dimentions.X / 2),
								                 {-1, 0},
								                 *CollisionEntity);
							if (disp) {
								Entity->Position.X += disp;
								Entity->Velocity.X = 0;
							}
						}
			
						{ // Right Leg
							int disp =
								EntityCollisionCheck (
								                  {(int)(Entity->Position.X),
								                   (int)(Entity->Position.Y - Entity->Dimentions.Y/2 + 1)},
								                  (int)(Entity->Dimentions.X / 2),
								                  {1, 0},
								                  *CollisionEntity);
							if (disp) {
								Entity->Position.X -= disp;
								Entity->Velocity.X = 0;
							}
						}
					}

					// Can Jump
					if (Entity->Velocity.Y <= 0) {
						int DeltaYr =
							EntityCollisionCheck (
							                  {(int)(Entity->Position.X + Entity->Dimentions.X/2 - 2),
							                   (int)(Entity->Position.Y)},
							                  (int)(Entity->Dimentions.Y/2 + 12),
							                  {0, -1},
							                  *CollisionEntity);
				
						int DeltaYl =
							EntityCollisionCheck (
							                  {(int)(Entity->Position.X - Entity->Dimentions.X/2 + 2),
							                   (int)(Entity->Position.Y)},
							                  (int)(Entity->Dimentions.Y/2 + 12),
							                  {0, -1},
							                  *CollisionEntity);

						if (DeltaYl != 0 || DeltaYr != 0) {
							Entity->CanJump = true;
						}
					}
		
					//Head
					if (Entity->Velocity.Y >= 0) { // if we're rising, lets bonk our head.
						int deltaYr =
							EntityCollisionCheck(
							                 {(int)(Entity->Position.X + Entity->Dimentions.X/2 - 2),
							                  (int)(Entity->Position.Y)},
							                 (int)(Entity->Dimentions.Y/2 + 1),
							                 {0, 1},
							                 *CollisionEntity);
						int deltaYl =
							EntityCollisionCheck(
							                 {(int)(Entity->Position.X - Entity->Dimentions.X/2 + 2),
							                  (int)(Entity->Position.Y)},
							                 (int)(Entity->Dimentions.Y/2 + 1),
							                 {0, 1},
							                 *CollisionEntity);
			
						if (deltaYr != 0 || deltaYl != 0) {
							Entity->Velocity.Y = 0;
				
							if (deltaYr > deltaYl)
								Entity->Position.Y -= deltaYr;
							else
								Entity->Position.Y -= deltaYl;

							for (s32 Count = 0; Count < 40; Count++) {
								iv2 CheckPos = {
									(s32)RandomFloat(Entity->Position.X - Entity->Dimentions.X/2, Entity->Position.X + Entity->Dimentions.X/2),
									(s32)(Entity->Position.Y + Entity->Dimentions.Y/2)};
								SpawnParticalAroundEntity(CheckPos, {0, 1}, 3, *CollisionEntity, 0.32);
							}
						}
					}
					else { //if we're falling, lets push us out of walls.
						{ // Left Head
							int disp =
								EntityCollisionCheck(
								                 {(int)(Entity->Position.X),
								                  (int)(Entity->Position.Y + Entity->Dimentions.Y/2 - 1)},
								                 (int)(Entity->Dimentions.X / 2),
								                 {-1, 0},
								                 *CollisionEntity);
							if (disp) {
								Entity->Position.X += disp;
								Entity->Velocity.X = 0;
							}
						}
			
						{ // Right Head
							int disp =
								EntityCollisionCheck(
								                 {(int)(Entity->Position.X),
								                  (int)(Entity->Position.Y + Entity->Dimentions.Y/2 - 1)},
								                 (int)(Entity->Dimentions.X / 2),
								                 {1, 0},
								                 *CollisionEntity);
							if (disp) {
								Entity->Position.X -= disp;
								Entity->Velocity.X = 0;
							}
						}
					}
				}
			}
			
		}

		if (Entity->Flags & EFLAG_Controlled) {
			if (InputState.Current.Up && !InputState.Prevous.Up && (Entity->CanJump || Entity->Grounded)) {
				Entity->Velocity.Y = 300;
			}

			for (s32 WinIndex = 0; WinIndex < ArraySize(EntityList); WinIndex++) {
				entity * const WinEntity = &EntityList[WinIndex];
				const iv2 TestPos = {(s32)Entity->Position.X, (s32)Entity->Position.Y};

				if (WinEntity->Flags & EFLAG_WinLevel) {
					if (IsInsideEntity(TestPos, *WinEntity)) {
						GameState.LevelWon = true;
						EntityList[GameState.PlayerEntity].Flags |= EFLAG_RenderRect; // @TODO(Michael) This assumes that [0] is always the player, it might not be!
					}
				}
			}
		}

		// Rendering Flags
		
		if (Entity->Flags & EFLAG_RenderRect) {
			quad_fill_instance_data Data {
				Entity->Position,
				Entity->Dimentions,
				Entity->Color
			};
			Render_EnqueueQuadFill(Data);
		}

		if ((Entity->Flags & EFLAG_RenderRectWhenWon) && GameState.LevelWon) {
			quad_fill_instance_data Data {
				Entity->Position,
				Entity->Dimentions,
				Entity->Color
			};
			Render_EnqueueQuadFill(Data);
		}
	}

	// @TODO(Michael) This Draw call shouldn't have to be here, but until we get the depthbuffer working it does.
	if (GameState.LevelWon) {
		Render_EnqueueQuadSample(
		  {
				hmm_v3{(f32)WindowWidth/2.f, (f32)WindowHeight/2.f, 1},
				hmm_v2{(f32)WindowWidth, (f32)WindowHeight},
				2,
				hmm_v2{0.0, 0.0},
				hmm_v2{1.0, 0.0},
				hmm_v2{0.0, 1.0},
				hmm_v2{1.0, 1.0},
		  });
	}

	// @TODO(Michael) This Draw call shouldn't have to be here, but until we get the depthbuffer working it does.
	Render_EnqueueQuadSample(
	  {
			hmm_v3{(f32)WindowWidth/2.f, (f32)WindowHeight/2.f, 0},
			hmm_v2{(f32)WindowWidth, (f32)WindowHeight},
			4,
			hmm_v2{0.0, 0.0},
			hmm_v2{1.0, 0.0},
			hmm_v2{0.0, 1.0},
			hmm_v2{1.0, 1.0},
	  });
	
	for (auto Iter = ParticalList.begin(); Iter != ParticalList.end(); Iter++) {
		if (Iter->Flags & PFLAG_SimMovement) {
			Iter->Position.XY += Iter->Velocity * DeltaTime;
		}

		if (Iter->Flags & PFLAG_RenderRect) {
			quad_fill_instance_data Data {
				Iter->Position,
				Iter->Dimentions,
				Iter->Color
			};
			Render_EnqueueQuadFill(Data);
		}

		if (Iter->Flags & PFLAG_DoLifeTime) {
			Iter->CurTime += DeltaTime;
			if (Iter->CurTime > Iter->LifeTime) {
				Iter = ParticalList.erase(Iter);
			}
		}

		if (Iter == ParticalList.end()) { break; }
	}
	
	Render_Draw();
}
