#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <NCSEcw.h>
#include <NCSECWClient.h>
#include <NCSErrors.h>

uint_fast8_t const bands = 3;
Display *D;
int S;
Window W;
GC Gc;
uint_fast16_t WX = 200, WY = 200;

#define NCS(wut) \
	e = NCS##wut; \
	if (e != NCS_SUCCESS) { \
		printf("Error = %s\n", NCSGetErrorText(e)); \
	}

NCSEcwReadStatus showcb(NCSFileView *fw) {
	NCSError e;
	NCSFileViewSetInfo *vi;

	NCS(cbmGetViewInfo(fw, &vi));

	//printf("X %i Y %i avblks %i avblk %i mblk %i vblk %i\n", vi->nSizeX, vi->nSizeY, vi->nBlocksAvailableAtSetView, vi->nBlocksAvailable, vi->nMissedBlocksDuringRead, vi->nBlocksInView);
	uint_fast32_t bpl = vi->nSizeX * 4;
	uint8_t *img = malloc(bands * vi->nSizeY * bpl);
	assert(img);
	
	uint8_t *img_ = img, *img__ = malloc(bands * bpl);
	for (uint_fast32_t y = vi->nSizeY; y != 0; y--) {
		NCSEcwReadStatus s = NCScbmReadViewLineBGR(fw, img__); 
		assert(s == 0);
		for (uint_fast32_t x = 0; x < vi->nSizeX; x++) {
			img_[x * 4] = img__[x * 3];
			img_[x * 4 + 1] = img__[x * 3 + 1];
			img_[x * 4 + 2] = img__[x * 3 + 2];
		}
		//printf("status %i\n", s);

		img_ += bpl;
	}
	free(img__);

	//printf("%i %i %i\n", DefaultDepth(D, S), WX, WY);
	XLockDisplay(D);
	XImage *xi = XCreateImage(D, DefaultVisual(D, S), DefaultDepth(D, S), ZPixmap, 0, (void *)img, vi->nSizeX, vi->nSizeY, 32, bpl);
	assert(xi);
	XPutImage(D, W, Gc, xi, 0, 0, 0, 0, WX, WY);
	XFlush(D);
	XDestroyImage(xi);
	XUnlockDisplay(D);
	return NCSECW_READ_OK;
}

void initX() {
	XInitThreads();
	D = XOpenDisplay(NULL);
	assert(D);
	S = DefaultScreen(D);
	W = XCreateSimpleWindow(D, RootWindow(D, S), 10, 10, WX, WY, 1,
                           BlackPixel(D, S), WhitePixel(D, S));
	Gc = DefaultGC(D, S);
	assert(Gc);
	XSelectInput(D, W, ExposureMask | KeyPressMask | StructureNotifyMask);
	XMapWindow(D, W);
}

void usage(char *argv[]) {
	fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
	exit(1);
}

int main(int argc, char *argv[]) {
	if (argc < 2) usage(argv);

	NCSecwInit();
	NCSError e;
	NCSFileView *fw;
	NCSFileViewFileInfo *fi;
	NCSFileViewSetInfo *si;

	initX();

	NCS(cbmOpenFileView(argv[1], &fw, showcb));
	NCS(cbmGetViewFileInfo(fw, &fi));
	printf("%i bands\n", fi->nBands);
	NCS(cbmGetViewInfo(fw, &si));
	uint32_t *bl = malloc(4 * bands);
	for (uint_fast8_t i = 0; i < bands; i++)
		bl[i] = i;

	uint_fast32_t x = 0, y = 0, w = fi->nSizeX - 1, h = fi->nSizeY - 1;
//	NCS(cbmSetFileView(fw, 3, bl, x, y, w, h, WX, WY));

	bool run = true;
	while (run) {
		XEvent ev;
		XNextEvent(D, &ev);
		switch (ev.type) {
			case Expose:
			//	XFillRectangle(D, W, DefaultGC(D, S), 20, 20, 10, 10);
				break;
			case ConfigureNotify:
				WX = ev.xconfigure.width;
				WY = ev.xconfigure.height;
				//NCS(cbmSetFileView(fw, bands, bl, x, y, w, h, WX, WY));
				break;
			case KeyPress: {
				XLockDisplay(D);
				KeySym ks = XKeycodeToKeysym(D, (KeyCode)ev.xkey.keycode, 0);
				XUnlockDisplay(D);
				int_fast32_t t;
				switch (ks) {
					case XK_Up:
						t = y - h / 4;
						y = t;
						break;
					case XK_Down:
						t = y + h / 4;
						y = t;
						break;
					case XK_Left:
						t = x - w / 4;
						x = t;
						break;
					case XK_Right:
						t = x + w / 4;
						x = t;
						break;
					case XK_plus:
					case XK_equal:
						w = w * 2 / 3;
						h = h * 2 / 3;
						x += w / 3;
						y += h / 3;
						printf("zoom %lf%%\n", 100.0 * w / fi->nSizeX);
						break;
					case XK_minus:
						x -= w / 3;
						y -= h / 3;
						w = w * 3 / 2;
						h = h * 3 / 2;
						printf("zoom %lf%%\n", 100.0 * (w + 1) / fi->nSizeX);
						break;
					case 'i':
						printf("zoom %lf%%\n", 100.0 * (w + 1) / fi->nSizeX);
						break;
					case 'q':
						run = false;
						break;
					default:
						printf("keysym %lx\n", ks);
				}
				NCS(cbmSetFileView(fw, bands, bl, x, y, w + x, h + y, WX, WY));
				}
				break;
			default:
				break;
		}
	}

	XLockDisplay(D);
	XCloseDisplay(D);

	return 0;
}
