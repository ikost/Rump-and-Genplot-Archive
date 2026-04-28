/* ----- Support routines --------- */
size_t rll_encode(unsigned char *src, size_t nbytes, unsigned char *dest);
size_t tiff_encode(unsigned char *src, size_t nbytes, unsigned char *dest, int flags);
size_t delta_encode(unsigned char *src, size_t nbytes, unsigned char *dest, unsigned char *lastrow);

/* ----- Drivers ----------- */
int	LaserJet(int key);			/* Really all LOGICAL */
int	LaserJet4(int key);
int	DeskJet(int key);
int	DeskJetC(int key);
int	PaintJet(int key);
int	QuietJet(int key);
int	Epson(int key);
int	Okidata(int key);
int	Pin24(int key);
int	TiffOut(int key);
