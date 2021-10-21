 #include "External/HandmadeMath.h"
#define STB_IMAGE_IMPLEMENTATION
#include "External/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#include "Blind_Render.cpp"
#include "Blind_Map.cpp"

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

	// Visual
	// @TODO(Michael) replace/add information about what texture this should use. Likely will be 4 texture coords and 1 index into our texture array?
	hmm_v3 Color;
};

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

global_var entity EntityList[100] = {};

const u32 ParticalMax = 5000;
global_var std::vector<partical> ParticalList;

struct game_state {
	u32 *TileMap[MapHeight][MapWidth];
	b8 CanDraw;
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
s32 MapColisionCheck (iv2 Start, s32 Length, iv2 Direction, u32* TileMap[][MapWidth]) {
	for (int Index = 1; Index <= Length; Index++) {
		iv2 Position = {(Start.X + Index * Direction.X), (Start.Y + Index * Direction.Y)};

		iv2 Tile = {Position.X / 32, 29 - Position.Y / 32};
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

void SpawnParticalAroundColision(iv2 StartPoint, iv2 Direction, s32 Length) {
	s32 Offset = (Length - 1) - MapColisionCheck(StartPoint, Length, Direction, GameState.TileMap);
	
	if (Offset != (Length - 1)) {
		hmm_v2 fOffset = {(f32)(Offset * Direction.X), (f32)(Offset * Direction.Y)};
		hmm_v2 fStartPoint = {(f32)StartPoint.X, (f32)StartPoint.Y};
		
		partical Partical = {};
		Partical.Flags = PFLAG_DoLifeTime | PFLAG_RenderRect | PFLAG_SimMovement;
		Partical.Position.XY = fStartPoint + fOffset;
		Partical.Position.Z = 4;
		Partical.Velocity = hmm_v2{RandomFloat(-5, 5), RandomFloat(-5, 5)};
		Partical.Dimentions = hmm_v2{1, 1};
		Partical.Color = hmm_v3{0, RandomFloat(0.8, 1.0), RandomFloat(0.0, 0.6)};
		Partical.LifeTime = 0.5;
		Partical.CurTime = 0;
		
		ParticalList.push_back(Partical);
	}
}

void BlindSimulateAndRender(f32 DeltaTime, input_state InputState) {

	local_persist b8 Init = false;
	if (!Init) {
		Init = true;
		
		EntityList[0].Flags |= EFLAG_Controlled | EFLAG_DoGravity | EFLAG_SimMovement | EFLAG_RenderRect | EFLAG_CollideWithMap;
		EntityList[0].Position = { 32, 256, 5};
		EntityList[0].Dimentions = {24, 32};
		EntityList[0].Color = {1.0, 0.5, 0.0};
		
		memcpy(GameState.TileMap, TEST_TILE_MAP, sizeof(TEST_TILE_MAP));
		InitMap(GameState.TileMap);

		GameState.CanDraw = true;

		ParticalList.reserve(ParticalMax);
	}

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
						*(u32*)Pixel = 0xFFFF0000;
					}
					
				}
			}
			
		}
		Render_UpdateTextureArray(4, UserDrawTextureData, WindowWidth * 4);
	}
	
	for (s32 Index = 0; Index < ArraySize(EntityList); Index++) {
		entity * const Entity = &EntityList[Index];
		
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
	
		if (Entity->Flags & EFLAG_DoGravity) {
			if (!Entity->Grounded) {
				Entity->Acceleration.Y = -600;
			}
			else {
				Entity->Acceleration.Y = 0;
			}
		}
	
		if (Entity->Flags & EFLAG_SimMovement) {
			// classic kinematic eqs
			// pos += Velocity * delta + acl * delta * delta * 0.5
			Entity->Position.XY += (Entity->Velocity * DeltaTime) + (Entity->Acceleration * DeltaTime * DeltaTime * 0.5);
			// vel += acl * delta
			Entity->Velocity += Entity->Acceleration * DeltaTime;
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
						SpawnParticalAroundColision(CheckPos, {-1, 0}, 4);
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
						SpawnParticalAroundColision(CheckPos, {1, 0}, 4);
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
						SpawnParticalAroundColision(CheckPos, {0, -1}, 8);
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
						SpawnParticalAroundColision(CheckPos, {0, 1}, 3);
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
		}

		if (Entity->Flags & EFLAG_Controlled) {
			if (InputState.Current.Up && !InputState.Prevous.Up && (Entity->CanJump || Entity->Grounded)) {
				Entity->Velocity.Y = 300;
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
	}

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
