#ifndef _PALETTE_H
#define _PALETTE_H

// 0000BBB0GGG0RRR0

// covert to 16bit VDP col. R,G,B 0-7 (3bits)
#define VDP_COL( r, g, b ) \
	((u16)(/*(0 << 12) | */(b << 9) | (g << 5) | (r << 1)))

// main palette
u16 myPal[16] = {
		VDP_COL( 0, 0, 0 ), VDP_COL( 2, 1, 1 ),
		VDP_COL( 1, 1, 3 ), VDP_COL( 2, 2, 2 ),
		VDP_COL( 4, 2, 1 ), VDP_COL( 1, 3, 1 ),
		VDP_COL( 6, 2, 2 ), VDP_COL( 3, 3, 3 ),

		VDP_COL( 2, 3, 6 ), VDP_COL( 6, 3, 1 ),
		VDP_COL( 4, 4, 5 ), VDP_COL( 3, 5, 1 ),
		VDP_COL( 6, 5, 4 ), VDP_COL( 3, 6, 6 ),
		VDP_COL( 6, 6, 2 ), VDP_COL( 6, 7, 6 )
};

// main palette darkened
u16 myPalDark[16] = {
		VDP_COL( 0, 0, 0 ), VDP_COL( 1, 0, 0 ),
		VDP_COL( 0, 0, 1 ), VDP_COL( 1, 1, 1 ),
		VDP_COL( 2, 1, 0 ), VDP_COL( 0, 1, 0 ),
		VDP_COL( 3, 1, 1 ), VDP_COL( 1, 1, 1 ),

		VDP_COL( 1, 1, 3 ), VDP_COL( 3, 1, 0 ),
		VDP_COL( 2, 2, 2 ), VDP_COL( 1, 2, 0 ),
		VDP_COL( 3, 2, 2 ), VDP_COL( 1, 3, 3 ),
		VDP_COL( 3, 3, 1 ), VDP_COL( 3, 3, 3 )
};

#endif // _PALETTE_H
