// ****************************************************************************
// VGA configuration — memory-optimized for 320x240 Nextion bridge
// (project-local; does not modify PicoVGA sources)
// ****************************************************************************

// 1 base layer only (no overlays for v1)
#define LAYERS   1
#define SEGMAX   4
#define STRIPMAX 8

#define MAXX     320
#define MAXY     240
#define MAXLINE  525

// Scanline render buffers
#define DBUF0_MAX (MAXX+8)
#define CBUF0_MAX ((MAXX+24)/4)

#define DBUF1_MAX (MAXX+8)
#define CBUF1_MAX ((MAXX+24)/4)
#define DBUF2_MAX (MAXX+8)
#define CBUF2_MAX ((MAXX+24)/4)
#define DBUF3_MAX (MAXX+8)
#define CBUF3_MAX ((MAXX+24)/4)

#if LAYERS==1
#define DBUF_MAX DBUF0_MAX
#define CBUF_MAX CBUF0_MAX
#elif LAYERS==2
#define DBUF_MAX (DBUF0_MAX+DBUF1_MAX)
#define CBUF_MAX (CBUF0_MAX+CBUF1_MAX)
#elif LAYERS==3
#define DBUF_MAX (DBUF0_MAX+DBUF1_MAX+DBUF2_MAX)
#define CBUF_MAX (CBUF0_MAX+CBUF1_MAX+CBUF2_MAX)
#elif LAYERS==4
#define DBUF_MAX (DBUF0_MAX+DBUF1_MAX+DBUF2_MAX+DBUF3_MAX)
#define CBUF_MAX (CBUF0_MAX+CBUF1_MAX+CBUF2_MAX+CBUF3_MAX)
#else
#error Unsupported number of layers!
#endif

#define VGA_GPIO_FIRST 0
#define VGA_GPIO_NUM 9
#define VGA_GPIO_OUTNUM 8
#define VGA_GPIO_LAST (VGA_GPIO_FIRST+VGA_GPIO_NUM-1)
#define VGA_GPIO_SYNC 8

#define VGA_PIO pio0
#define VGA_SM0 0
#define VGA_SM1 1
#define VGA_SM2 2
#define VGA_SM3 3
#define VGA_SM(layer) (VGA_SM0+(layer))

#if LAYERS==1
#define VGA_SMALL B0
#elif LAYERS==2
#define VGA_SMALL (B0+B1)
#elif LAYERS==3
#define VGA_SMALL (B0+B1+B2)
#elif LAYERS==4
#define VGA_SMALL (B0+B1+B2+B3)
#else
#error Unsupported number of layers!
#endif

#define VGA_DMA 0
#define VGA_DMA_CB0 (VGA_DMA+0)
#define VGA_DMA_PIO0 (VGA_DMA+1)
#define VGA_DMA_CB1 (VGA_DMA+2)
#define VGA_DMA_PIO1 (VGA_DMA+3)
#define VGA_DMA_CB2 (VGA_DMA+4)
#define VGA_DMA_PIO2 (VGA_DMA+5)
#define VGA_DMA_CB3 (VGA_DMA+6)
#define VGA_DMA_PIO3 (VGA_DMA+7)

#define VGA_DMA_CB(layer) (VGA_DMA_CB0+(layer)*2)
#define VGA_DMA_PIO(layer) (VGA_DMA_PIO0+(layer)*2)

#define VGA_DMA_NUM (LAYERS*2)
#define VGA_DMA_FIRST VGA_DMA
#define VGA_DMA_LAST (VGA_DMA_FIRST+VGA_DMA_NUM-1)
