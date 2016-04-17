#include <genesis.h>
#include <sram.h>

#include "src/boot/rom_head.h"

// palettes
#include "palette.h"
// tile data
#include "tiles.h"

// sounds data
#include "sounds.h"
// xgm pcm ids
#define SND_ID_BRIK 64
#define SND_ID_WALL 65
#define SND_ID_PAD 66
#define SND_ID_DEAD 67

// is there already valid save data on sram
u8 saveInit = FALSE;

// sprites
// spr0 - ball
SpriteDef spr0 = {152, 200-16, TILE_ATTR_FULL( PAL0, 0, 0, 0, 1 ), SPRITE_SIZE( SPR0_W, SPR0_H ), 1};
// fix16 positions
fix16 spr0x = 0;
fix16 spr0y = 0;
u8 spr0vel = 0;
// angle used to calulate normalized direction (0-1024)
u16 spr0ang = 0;
fix16 spr0dir[2] = {0, 0};
s8 spr0rotdir = 1;
u8 spr0rot = 0;

// spr_pad
SpriteDef spr_pad_left = {160, 200, TILE_ATTR_FULL( PAL0, 0, 0, 0, 5 ), SPRITE_SIZE( SPR_PAD_W, SPR_PAD_H ), 2};
SpriteDef spr_pad_right = {160+32, 200, TILE_ATTR_FULL( PAL0, 0, 0, 1, 5 ), SPRITE_SIZE( SPR_PAD_W, SPR_PAD_H ), 0};
s8 padVel = 0;
u8 padVelM = 1;
s8 padVelY = 0;

// spr_brik0 ????
u16 brik0idx = 0;
u16 brik1idx = 0;
// [y][x]
u8 brik_field[7][12] = {
	{1,1,1,1,1,1,1,1,1,1,1,1},
	{1,0,0,0,0,2,2,0,0,0,0,1},
	{1,0,0,1,1,1,1,1,1,0,0,1},
	{1,0,1,1,1,1,1,1,1,1,0,1},
	{1,0,2,2,2,1,1,2,2,2,0,1},
	{0,0,2,2,2,0,0,2,2,1,0,0},
	{0,1,1,1,1,0,0,1,1,1,1,0}
};

// gameplay
u8 waitLaunch = 1;
u16 score = 0;
s8 noBrikHit = 0;

void UpdateBrikField( void );
void RemoveBrik( u8 x, u8 y );

// sets ball angle and updates dir vec
void SetBallAng( u16 angle ) {
	do {
	    spr0ang = angle;
	    spr0dir[0] = cosFix16( spr0ang );
		spr0dir[1] = sinFix16( spr0ang );
		angle = (angle + 1) % 1023;
	} while ( spr0dir[1] == 0 );
}

void UpdateBallRot( void ) {
    if ( spr0rot == 1 || spr0rot == 2 )
        spr0.tile_attr |= (1<<11);
    else spr0.tile_attr &= ~(1<<11);
    if ( spr0rot == 2 || spr0rot == 3 )
        spr0.tile_attr |= (1<<12);
    else spr0.tile_attr &= ~(1<<12);
}

void SaveScore( void ) {
	if ( score > 0 ) {
		SRAM_enable();
		SRAM_writeWord( 4, score );
		SRAM_disable();
	}
}

void PrepLaunch( void ) {
    // reset position
    spr0y = intToFix16( screenHeight - 40 );
    spr0x = intToFix16( 152 );
    spr0.posx = fix16ToInt( spr0x );
    spr0.posy = fix16ToInt( spr0y );

	spr0vel = 0;
	noBrikHit = 0;
	padVelM = 1;
	// random start angle -45 to +45 degrees (640-896)
	SetBallAng( random() % 256 + 640 );
	// clear flipping
	spr0.tile_attr &= ~((1<<11)|(1<<12));
	spr0rot = 0;
	waitLaunch = 1;
	SaveScore();
}

// input callback
void HandleJoyInput( u16 joy, u16 changed, u16 state ) {
	// p1
	if ( joy == JOY_1 ) {
		if ( JOY_getJoypadType( joy ) == JOY_TYPE_PAD6 ) {
			// using 6btn
		}

		// launch
		if ( waitLaunch && (state & BUTTON_START) ) {
			waitLaunch = 0;
			spr0vel = 3;
			VDP_clearTextLine( screenHeight / 8 - 1 );
			//RemoveBrik( 11, 2 );
		}

		// pressed
		if ( state & BUTTON_RIGHT ) {
			padVel = 4;
		} else if ( state & BUTTON_LEFT ) {
			padVel = -4;
		} else {
			// released
			if ( (changed & BUTTON_RIGHT) || (changed & BUTTON_LEFT) ) {
				padVel = 0;
			}
		}
	}
}

void DrawBGTile( u16 idx, u8 tw, u8 th, u8 x, u8 y ) {
	u8 cnt = 0;
	for ( u8 cy = 0; cy < th; ++cy )
		for ( u8 cx = 0; cx < tw; ++cx ) {
			VDP_setTileMapXY( APLAN, TILE_ATTR_FULL( PAL0, 0, 0, 0, idx + cnt ), x + cx, y + cy );
			++cnt;
		}
}

void DrawBGTileFill( u8 idx, u8 tw, u8 th, u8 x, u8 y, u8 w, u8 h ) {
	for ( u8 cy = 0; cy < h; ++cy )
		for ( u8 cx = 0; cx < w; ++cx )
			DrawBGTile( idx, tw, th, x + cx * tw, y + cy * th );
}

// toggle flipping. 0 = h
#define FLIP_SPR( spr, hv ) \
	(spr.tile_attr ^= ((hv == 0) ? 1<<11 : 1<<12))

// returns reflected angle based on current angle and normal
u16 ReflectAngle( u16 angle, u16 normal ) {
    u16 opang = (angle + 512) % 1024;
    return (normal - (opang - normal)) % 1024;
}

// redraw entire brikfield
void UpdateBrikField( void ) {
	for ( u8 y = 0; y < sizeof (brik_field) / sizeof (brik_field[0]); ++y ) {
		for ( u8 x = 0; x < sizeof (brik_field[y]); ++x ) {
			if ( brik_field[y][x] == 1 )
				DrawBGTile( brik0idx, SPR_BRIK_W, SPR_BRIK_H, 2+(x*SPR_BRIK_W), y*SPR_BRIK_H );
			else if ( brik_field[y][x] == 2 )
				DrawBGTile( brik1idx, SPR_BRIK_W, SPR_BRIK_H, 2+(x*SPR_BRIK_W), y*SPR_BRIK_H );
			//else DrawBGTile( 0x0580, SPR_BRIK_W, SPR_BRIK_H, 2+(x*SPR_BRIK_W), y*SPR_BRIK_H );
		}
	}
}

// set brik array data to 0 and draw transparent tile in spot
void RemoveBrik( u8 x, u8 y ) {
    brik_field[y][x] = 0;
    DrawBGTile( 0x0580, SPR_BRIK_W, SPR_BRIK_H, 2+(x*SPR_BRIK_W), y*SPR_BRIK_H );
}

// checks ball againts brik field for collisions
void DoBrikCollision( void ) {
	u8 bw = SPR_BRIK_W * 8;
	u8 bh = SPR_BRIK_H * 8;
	for ( u8 y = 0; y < sizeof (brik_field) / sizeof (brik_field[0]); ++y ) {
		u16 by = y * bh;
		for ( u8 x = 0; x < sizeof (brik_field[y]); ++x ) {
			u16 bx = 16 + (x * bw);
			if ( brik_field[y][x] > 0 ) {
				if ( !(bx > spr0.posx + 16
					|| bx + bw < spr0.posx
					|| by > spr0.posy + 16
					|| by + bh < spr0.posy) ) {
					RemoveBrik( x, y );
					u16 colx = (spr0.posx > bx) ? spr0.posx : bx;
					u16 coly = (spr0.posy > by) ? spr0.posy : by;
					u16 colx2 = (spr0.posx + 16) < (bx + bw) ? (spr0.posx + 16) : (bx + bw);
					u16 coly2 = (spr0.posy + 16) < (by + bh) ? (spr0.posy + 16) : (by + bh);
					u16 colw = colx2 - colx;
					u16 colh = coly2 - coly;
					if ( colw > colh ) {
						if ( spr0dir[1] < 0 ) SetBallAng( ReflectAngle( spr0ang, 256 ) );
						else if ( spr0dir[1] > 0 ) SetBallAng( ReflectAngle( spr0ang, 768 ) );
						else SetBallAng( (spr0ang + 512) % 1024 );
					} else if ( colh > colw ) {
						if ( spr0dir[0] < 0 ) SetBallAng( ReflectAngle( spr0ang, 0 ) );
						else if ( spr0dir[0] > 0 ) SetBallAng( ReflectAngle( spr0ang, 512 ) );
						else SetBallAng( (spr0ang + 512) % 1024 );
					} else SetBallAng( (spr0ang + 512) % 1024 );
					++score;
					static u8 chan = SOUND_PCM_CH2;
					if ( SND_isPlayingPCM_XGM( (1 << chan) ) != 0 )
						chan = (chan + 1) % 4;
					SND_startPlayPCM_XGM( SND_ID_BRIK, 0, chan );
					if ( noBrikHit < 0 ) {
						spr0vel /= 2;
						padVelM = 1;
					}
					noBrikHit = 0;
					return;
				}
			}
		}
	}
}

int main( void ) {
    // mark sram as valid data
    SRAM_enable();
    u32 sramMagic = SRAM_readLong( 0 );
    // valid sram was found
    if ( sramMagic == 0x00C0FFEE ) saveInit = TRUE;
    else {
        // sram has never been used yet
        SRAM_writeLong( 0, 0x00C0FFEE );
    }
    SRAM_disable();

	// joypad
	JOY_init();
	JOY_setEventHandler( HandleJoyInput );

	if ( ROM_REGION == ROM_EUR || ROM_REGION == ROM_JAP )
        VDP_setScreenHeight240();
	else
        VDP_setScreenHeight224();
	//VDP_setHilightShadow( 1 );

	VDP_setPalette( 0, myPal );
	VDP_setPalette( 3, palette_grey );
	VDP_setTextPalette( 3 );
	VDP_setBackgroundColor( 0 );

	// load tile data on vram
	u8 nextIdx = 0;
	VDP_loadTileData( bgtile, nextIdx, 1, 0 );
	nextIdx += 1;
	VDP_loadTileData( spr0_tiles, nextIdx, SPR0_W * SPR0_H, 0 );
	nextIdx += SPR0_W * SPR0_H;
	VDP_loadTileData( spr_pad_tiles, nextIdx, SPR_PAD_W * SPR_PAD_H, 0 );
	nextIdx += SPR_PAD_W * SPR_PAD_H;
	VDP_loadTileData( spr_brik0_tiles, nextIdx, SPR_BRIK_W * SPR_BRIK_H, 0 );
	brik0idx = nextIdx;
	nextIdx += SPR_BRIK_W * SPR_BRIK_H;
	VDP_loadTileData( spr_brik1_tiles, nextIdx, SPR_BRIK_W * SPR_BRIK_H, 0 );
	brik1idx = nextIdx;
	nextIdx += SPR_BRIK_W * SPR_BRIK_H;
	// write tile on plane
	// pal, pri, v, h, idx
	//VDP_setTileMapXY( APLAN, TILE_ATTR_FULL( PAL0, 0, 0, 0, 0 ), 10, 10 );
	//VDP_setTileMapXY( APLAN, TILE_ATTR_FULL( PAL0, 0, 0, 0, 1 ), 11, 10 );
	//VDP_fillTileMapRect( APLAN, TILE_ATTR_FULL( PAL0, 0, 0, 0, 1 ), 5, 5, 8, 8 );

	// idx, tw, th, xp, yp
	//DrawBGTile( 1, 1, 2, 5, 5 );
	//DrawBGTileFill( 1, 2, 1, 5, 5, 8, 8 );
	UpdateBrikField();

	// sprites
	//VDP_setSprite( 0, 156, 108, SPRITE_SIZE( 2, 1 ), TILE_ATTR_FULL( PAL0, 0, 0, 0, 1 ), 0 );

	spr0.posy = screenHeight - 40;
	spr_pad_left.posy = screenHeight - 24;
	//spr_pad_right.posy = spr_pad_left.posy;
	VDP_setSpriteP( 0, &spr0 );
	VDP_setSpriteP( 1, &spr_pad_left );
	VDP_setSpriteP( 2, &spr_pad_right );

	// setup
    PrepLaunch();

    // load save
    if ( saveInit == TRUE ) {
        SRAM_enableRO();
        score = SRAM_readWord( 4 );
        SRAM_disable();
    }

    //SND_startPlay_XGM( xgm_bg );
    SND_setPCM_XGM( SND_ID_BRIK, snd_brik, sizeof snd_brik );
    SND_setPCM_XGM( SND_ID_WALL, snd_wall, sizeof snd_wall );
    SND_setPCM_XGM( SND_ID_PAD, snd_pad, sizeof snd_pad );
    SND_setPCM_XGM( SND_ID_DEAD, snd_dead, sizeof snd_dead );

	while ( 1 ) {
		// logic
		spr_pad_left.posx += (padVel * padVelM);
		if ( padVelY != 0 ) spr_pad_left.posy += padVelY;
		if ( spr_pad_left.posy + 16 > screenHeight ) padVelY = -2;
		if ( spr_pad_left.posy < screenHeight - 24 ) {
			spr_pad_left.posy = screenHeight - 24;
			padVelY = 0;
		}

		// pad bounding
		if ( spr_pad_left.posx < 0 ) spr_pad_left.posx = 0;
		if ( spr_pad_left.posx + 64 > screenWidth ) spr_pad_left.posx = screenWidth - 64;

		// keep right half aligned
		spr_pad_right.posx = spr_pad_left.posx + 32;
		spr_pad_right.posy = spr_pad_left.posy;

		VDP_setSpriteP( 1, &spr_pad_left );
		VDP_setSpriteP( 2, &spr_pad_right );

		// death flash
		static u8 deathFlash = 0;

		if ( waitLaunch == 0 ) {
			spr0x = fix16Add( spr0x, fix16Mul( intToFix16( spr0vel ), spr0dir[0] ) );
			spr0.posx = fix16ToInt( spr0x );
			spr0y = fix16Add( spr0y, fix16Mul( intToFix16( spr0vel ), spr0dir[1] ) );
			spr0.posy = fix16ToInt( spr0y );

			// left / right walls
			if ( spr0.posx >= screenWidth - 16 && spr0dir[0] > 0 ) { // right wall
				//spr0dir[0] = -spr0dir[0];
				SetBallAng( ReflectAngle( spr0ang, 512 ) );
				SaveScore();
				SND_startPlayPCM_XGM( SND_ID_WALL, 0, SOUND_PCM_CH1 );
				++noBrikHit;
			}
			else if ( spr0.posx <= 0 && spr0dir[0] < 0 ) { // left wall
				//spr0dir[0] = -spr0dir[0];
				SetBallAng( ReflectAngle( spr0ang, 0 ) );
				SaveScore();
				SND_startPlayPCM_XGM( SND_ID_WALL, 0, SOUND_PCM_CH1 );
				++noBrikHit;
			}

			// top / bottom walls
			if ( spr0.posy >= screenHeight - 16 && spr0dir[1] > 0 ) {
				// dead
				deathFlash = 1;
				VDP_fadePalTo( 0, myPalDark, 7, 1 );
				PrepLaunch();
				SND_startPlayPCM_XGM( SND_ID_DEAD, 0, SOUND_PCM_CH2 );
			}
			if ( spr0.posy <= 0 && spr0dir[1] < 0 ) { // top wall
				SetBallAng( ReflectAngle( spr0ang, 256 ) );
				SaveScore();
				SND_startPlayPCM_XGM( SND_ID_WALL, 0, SOUND_PCM_CH1 );
				++noBrikHit;
			}

			// pad collision
			if ( (spr0.posx < spr_pad_left.posx + 64) && (spr0.posx > spr_pad_left.posx - 16)
				&& (spr0.posy >= (screenHeight - 24) - 16) && (spr0dir[1] > 0) ) {
                // how far from center of pad?
                s8 cdist = (spr0.posx + 8) - (spr_pad_left.posx + 32);
                if ( cdist > 16 ) { // right side
                	SetBallAng( ReflectAngle( spr0ang, 832 ) );
                }
                else if ( cdist < -16 ) { // left side
                	SetBallAng( ReflectAngle( spr0ang, 704 ) );
                }
				else SetBallAng( ReflectAngle( spr0ang, 768 ) );
				padVelY = 2;
				SND_startPlayPCM_XGM( SND_ID_PAD, 0, SOUND_PCM_CH3 );
				//++noBrikHit;
			}

            static u8 rottimer = 0;
			++rottimer;
			if ( rottimer > 7 ) {
                if ( spr0ang > 768 || spr0ang < 256 ) spr0rotdir = 1;
                else {
                    if ( spr0rot == 0 ) spr0rot = 4;
                    spr0rotdir = -1;
                }
                spr0rot = (spr0rot+spr0rotdir) % 4;
                UpdateBallRot();
                rottimer = 0;
			}

			DoBrikCollision();
			VDP_setSpriteP( 0, &spr0 );
			if ( noBrikHit > 2 ) {
				spr0vel *= 2;
				padVelM = 2;
				noBrikHit = -100;
			}
		} else {
			VDP_drawText( "Press Start to launch.", 0, screenHeight / 8 - 1 );
			spr0x = intToFix16( spr_pad_left.posx + 24 );
			spr0.posx = fix16ToInt( spr0x );
			VDP_setSpriteP( 0, &spr0 );
		}

		// death flash
		if ( deathFlash ) {
			if ( VDP_isDoingFade() == FALSE ) {
				VDP_fadePalTo( 0, myPal, 15, 1 );
				deathFlash = 0;
			}
		}

		// draw "score"
		char str[8];
		intToStr( score, str, 1 );
		VDP_drawText( str, 0, 0 );

		VDP_updateSprites();

		{ // VDP_showFPS()
			//char str[8];
			fix32ToStr( getFPS_f(), str, 1 );
			VDP_clearText( screenWidth / 8 - 4, screenHeight / 8 - 1, 4 );
			VDP_drawText( str, screenWidth / 8 - 4, screenHeight / 8 - 1 );

			intToStr( SND_getCPULoad_XGM(), str, 1 );
			VDP_clearText( screenWidth / 8 - 3, screenHeight / 8 - 2, 3 );
			VDP_drawText( str, screenWidth / 8 - 3, screenHeight / 8 - 2 );
		}
		VDP_waitVSync();
	}

	return 0;
}
