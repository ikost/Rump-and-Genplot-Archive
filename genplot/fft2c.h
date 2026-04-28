/* fft2c.c */
void fft2c(REAL *rdata, REAL *idata, int npt, int dir);
void fft_real(REAL *y, int npt, int dir);
void fft_r2(REAL *rdata, REAL *idata, int npt, int dir);
void fft_full(REAL *rdata, REAL *idata, int npt);
void fft_auto(REAL *y, int npt);
void fft_conv(REAL *buf1, REAL *buf2, int npt, int mode);
void fft_filt(REAL *buffer, int npt);
void fft_snr(REAL snr);
void fft_pow_est(int type, REAL *y, int *npt);
void fft_shft(REAL *rdata, REAL *idata, int npt, REAL phase);
void fft_y_roll(REAL *y, int npt, int iroll);
int  fft_power_2(int npt, int nptmax);
