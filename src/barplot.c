/*
 *  forecast - query weather forecasts from forecast.io
 *  Copyright (C) 2015 Jens John <dev@2ion.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "forecast.h"
#include "barplot.h"

static void start_curses(const PlotCfg*);
static void end_curses(void);
static void barplot_scale(const double*, size_t, int, int*, double*, double*, double*);
static void barplot_legend(int dx, int dy, int height, double dmax, double dmin);
static double frac_of_day_mins(const struct tm *t);

double frac_of_day_mins(const struct tm *t) {
  return (t->tm_hour*60 + t->tm_min) /(24.0*1440.0);
}

void barplot_legend(int dx, int dy, int height, double dmax, double dmin) {
  const int rfac = dmin < 0.0 ? 2 : 1;

  attron(COLOR_PAIR(PLOT_COLOR_LEGEND));
  for(int y = dy; y <= dy + rfac*height; y++) {
    if(y == dy + height) { /* zero-baseline */
      attron(COLOR_PAIR(PLOT_COLOR_TEXTHIGHLIGHT));
      mvaddch(y, dx-2, '+');
      attroff(COLOR_PAIR(PLOT_COLOR_TEXTHIGHLIGHT));
      attron(COLOR_PAIR(PLOT_COLOR_LEGEND));
      mvprintw(y, dx-6, "0.0");
    } else if(y == dy) { /* y-axis maximum */
      mvaddch(y, dx-2, '|');
      mvprintw(y,
          dx-(snprintf(NULL, 0, "%.*f", 1, dmax)+3),
          "%.*f", 1, dmax);
    } else if(y == dy + 2*height) { /* y-axis minimum */
      mvaddch(y, dx-2, '|');
      mvprintw(y,
          dx-(snprintf(NULL, 0, "-%.*f", 1, dmax)+3),
          "-%.*f", 1, dmax);
    } else
      mvaddch(y, dx-2, '|');
  }
  attroff(COLOR_PAIR(PLOT_COLOR_LEGEND));
}

void start_curses(const PlotCfg *pc) {
  int default_color;

/*  setlocale(LC_ALL, ""); */

  /* if this call fails, the program will terminate */
  initscr();

  /* screen and echo setup */
  cbreak();
  noecho();
  nonl();
  intrflush(stdscr, FALSE);
  keypad(stdscr, TRUE);

  /* hide cursor */
  curs_set(0);

  /* use colors and, if possible, terminal default colors */
  start_color();
  default_color = use_default_colors() == OK ? -1 : 0;

  /* colors defined in the config file */
  init_pair(PLOT_COLOR_BAR,           default_color,                  pc->bar.color);
  init_pair(PLOT_COLOR_LEGEND,        pc->legend.color,               default_color);
  init_pair(PLOT_COLOR_TEXTHIGHLIGHT, pc->legend.texthighlight_color, default_color);
  init_pair(PLOT_COLOR_BAR_OVERLAY,   default_color,                  pc->bar.overlay_color);
  init_pair(PLOT_COLOR_PRECIP,        default_color,                  pc->precipitation.bar_color);
  init_pair(PLOT_COLOR_DAYLIGHT,      pc->daylight.color,             default_color);
}

void end_curses(void) {
  refresh();
  getch();
  endwin();
}

int terminal_dimen(int *rows, int *cols) {
  struct winsize w;

  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
    LERROR(0, errno, "ioctl()");
    return -1;
  }

  *rows = w.ws_row;
  *cols = w.ws_col;

  return 0;
}

void barplot_scale(const double *d, size_t dlen, int scaleheight, int *scaled, double *scalefac, double *max, double *min) {
  for(int i = 0; i < dlen; i++) {
    double m = fabs(d[i]);
    if(m > *max)
      *max = m;
    else if(m < *min)
      *min = m;
  }

  *scalefac = (double) scaleheight / (*max);

  for(int i = 0; i < dlen; i++) {
    double m = d[i] * (*scalefac);
    if(m < 0.0)
      scaled[i] = (int) ceil(m);
    else if(m > 0.0)
      scaled[i] = (int) floor(m);
    else
      scaled[i] = 0;
  }
}

void barplot(const PlotCfg *c, const double *d, size_t dlen) {
  int dlist[dlen];

  /* scale doubles -> int */

  double maxabs = 0.0;

  for(int i = 0; i < dlen; i++) {
    double m = fabs(d[i]);
    if(m > maxabs)
      maxabs = m;
  }

  const double fac = (double) c->height / maxabs;
  int maxdlist = 0;

  for(int i = 0; i < dlen; i++) {
    double m = d[i] * fac;
    if(m < 0.0)
      dlist[i] = (int) ceil(m);
    else if(m > 0.0)
      dlist[i] = (int) floor(m);
    else
      dlist[i] = 0;
    if(dlist[i] > maxdlist)
      maxdlist = dlist[i];
  }

  /* tic labels on y axis */
  /* FIXME: Maybe use the non-extreme tics in the legend, too? */

  double ticnames[c->height + 1];
  ticnames[0] = 0.0;
  for(int i = 1; i <= c->height; i++)
    ticnames[i] = (1.0/fac) * (double) i;

  /* curses */

  start_curses(c);

  const int dx = COLS/2 - (dlen * (c->bar.width + 1) - 1)/2;
  const int dy = LINES/2 - c->height;

  /* plot the decoration and legend */

  attron(COLOR_PAIR(2));
  for(int y = dy; y <= dy + 2*c->height; y++) {
    if(y == dy + c->height) { /* zero-baseline */
      attron(COLOR_PAIR(3));
      mvaddch(y, dx-2, '+');
      attroff(COLOR_PAIR(3));
      attron(COLOR_PAIR(2));

      mvprintw(y, dx-6, "0.0");
    } else if(y == dy) { /* y-axis maximum */
      mvaddch(y, dx-2, '|');
      mvprintw(y,
          dx-(snprintf(NULL, 0, "%.*f", 1, ticnames[c->height])+3),
          "%.*f", 1, ticnames[c->height]);
    } else if(y == dy + 2*c->height) { /* y-axis minimum */
      mvaddch(y, dx-2, '|');
      mvprintw(y,
          dx-(snprintf(NULL, 0, "-%.*f", 1, ticnames[c->height])+3),
          "-%.*f", 1, ticnames[c->height]);
    } else
      mvaddch(y, dx-2, '|');
  }
  attroff(COLOR_PAIR(2));

  int offset = 0;
  for(int i = 0; i < dlen; i++) {
    const int d = dlist[i] >= 0 ? 1 : -1;
    const int _offset = offset;
    char barlabel[5];

    snprintf(barlabel, 5, " %02d ", i);
    attron(COLOR_PAIR(2));
    mvprintw(dy + c->height, dx + i + offset, barlabel);
    attroff(COLOR_PAIR(2));

    for(int j = dx + i + offset; j < dx + i + c->bar.width + _offset; j++, offset++) {
      attron(COLOR_PAIR(1));
      for(int y = dy + c->height - dlist[i]; y != dy + c->height; y += d)
        mvaddch(y, j, ' ');
      attroff(COLOR_PAIR(1));
    }
  }

  /* display, and uninit curses */

  end_curses();
}

void barplot2(const PlotCfg *pc, const double *d, char **labels, size_t dlen, int bar_color) {
  int ds[dlen];
  double sfac, dmax, dmin;

  barplot_scale(d, dlen, pc->height, &ds[0], &sfac, &dmax, &dmin);

  start_curses(pc);

  const int dx = COLS/2 - (dlen * (pc->bar.width + 1) - 1)/2;
  const int dy = LINES/2 - pc->height;

  barplot_legend(dx, dy, pc->height, dmax, dmin);

  int offset = 0;
  for(int i = 0; i < dlen; i++) {
    const int delta = ds[i] >= 0 ? 1 : -1;
    const int _offset = offset;

    attron(COLOR_PAIR(PLOT_COLOR_LEGEND));
    mvprintw(dy + pc->height, dx + i + offset, "%s", labels[i]);
    attroff(COLOR_PAIR(PLOT_COLOR_LEGEND));

    for(int j = dx + i + offset; j < dx + i + pc->bar.width + _offset; j++, offset++) {
      attron(COLOR_PAIR(bar_color));
      for(int y = dy + pc->height - ds[i]; y != dy + pc->height; y += delta)
        mvaddch(y, j, ' ');
      attroff(COLOR_PAIR(bar_color));
    }
  }

  end_curses();
}

void barplot_overlaid(const PlotCfg *pc, const double *d1, const double *d2, char **labels, size_t dlen) {
  double  d[2*dlen];
  int     ds[2*dlen];
  double  sfac;
  double  dmax;
  double  dmin;

  memcpy(&d, d1, dlen * sizeof(double));
  memcpy(&d[dlen], d2, dlen * sizeof(double));

  barplot_scale(d, 2*dlen, pc->height, &ds[0], &sfac, &dmax, &dmin);

  start_curses(pc);

  const int dx = COLS/2 - (dlen * (pc->bar.width + 1) - 1)/2;
  const int dy = LINES/2 - pc->height;

  barplot_legend(dx, dy, pc->height, dmax, dmin);

  int offset = 0;
  for(int i = 0; i < dlen; i++) {
    const int _offset = offset;

    attron(COLOR_PAIR(PLOT_COLOR_LEGEND));
    mvprintw(dy + pc->height, dx + i + offset, "%s", labels[i]);
    attroff(COLOR_PAIR(PLOT_COLOR_LEGEND));

    for(int j = dx + i + offset; j < dx + i + pc->bar.width + _offset; j++, offset++) {
      for(int k = 0; k < 2; k++) {
        const int idx = i + k * dlen;
        const int d = ds[idx] >= 0 ? 1 : -1;
        const int barcoloridx = (k == 0) ? PLOT_COLOR_BAR : PLOT_COLOR_BAR_OVERLAY;

        attron(COLOR_PAIR(barcoloridx));
        for(int y = dy + pc->height - ds[idx]; y != dy + pc->height; y += d)
          mvaddch(y, j, ' ');
        attroff(COLOR_PAIR(barcoloridx));
      } // for k
    } // for j

  } // for i

  end_curses();
}

void barplot_daylight(const PlotCfg *pc, const int *times, size_t days) {
  int barwidth;
  double scalefac, min, max;
  /* If we were to do things correctly, we would need to loop over the
   * date formats of the current locale and determine their maximum
   * length */
  char date_label[days][255];
  char atime_label[days][255];
  char btime_label[days][255];
  int dlabel_max = 0;
  int alabel_max = 0;
  int blabel_max = 0;

  double data[2*days+2]; /* +2 for 0h-24h extremes */
  int di = 0;
  int data_scaled[2*days+2];

  for(int i = 0; i < days; i++)
    for(int j = 0; j < 3; j++) {
      int k = 3 * i + j;
      time_t t = times[k];
      struct tm *uxt = gmtime(&t);
      char *lptr, *fmt;
      int *comp;

      switch(j) {
        case 0:
          lptr = &date_label[k][0];
          fmt = pc->daylight.date_label_format;
          comp = &dlabel_max;
          break;
        case 1:
          lptr = &atime_label[k][0];
          fmt = pc->daylight.time_label_format;
          comp = &alabel_max;
          data[di++] = frac_of_day_mins((const struct tm*)uxt);
          break;
        case 2:
          lptr = &btime_label[k][0];
          fmt = pc->daylight.time_label_format;
          comp = &blabel_max;
          data[di++] = frac_of_day_mins((const struct tm*)uxt);
          break;
      }
      strftime(lptr, 255, (const char*)fmt, (const struct tm*)&uxt);
      if(strlen(lptr) > *comp)
        *comp = strlen(lptr);
    }

//  terminal_dimen(&rows, &cols);

  /* Scale over [0.0, 24.0]*60 */
  data[di++] = 0.0;
  data[di++] = 1440.0;
  barplot_scale((const double*)&data[0], 2*days+2, barwidth, &data_scaled[0], &scalefac, &max, &min);

  start_curses(pc);

  const int dx = 0.5 * (COLS - barwidth);
  const int dy = 0.5 * (LINES - days);
  barwidth = pc->daylight.width_frac * LINES > pc->daylight.width_max ?
    pc->daylight.width_max : pc->daylight.width_frac * LINES;

  LERROR(0, 0, "barwidth=%d", barwidth);

  for(int y = 0; y < days; y++) {
    /* plot background */
    for(int x = dx; x < dx + barwidth; x++)
      LERROR(0, 0, "y = %d, x = %d",dy +y, x);
//      mvaddch(dy + y, x, ' ');
    /* plot daylight */
    /*
    attron(COLOR_PAIR(PLOT_COLOR_DAYLIGHT));
    for(int x = dx + data_scaled[y]; x < dx + data_scaled[2*y]; x++)
      mvaddch(dy + y, x, ' ');
    attroff(COLOR_PAIR(PLOT_COLOR_DAYLIGHT));
    */
  }

  //end_curses();
  

  return;
}
