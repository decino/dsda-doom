/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:  Heads-up displays
 *
 *-----------------------------------------------------------------------------
 */

// killough 5/3/98: remove unnecessary headers

#include "doomstat.h"
#include "hu_stuff.h"
#include "hu_lib.h"
#include "st_stuff.h" /* jff 2/16/98 need loc of status bar */
#include "s_sound.h"
#include "dstrings.h"
#include "sounds.h"
#include "d_deh.h"   /* Ty 03/27/98 - externalization of mapnamesx arrays */
#include "g_game.h"
#include "r_main.h"
#include "p_inter.h"
#include "p_tick.h"
#include "p_map.h"
#include "sc_man.h"
#include "m_misc.h"
#include "r_main.h"
#include "lprintf.h"
#include "p_setup.h"
#include "e6y.h" //e6y
#include "dsda.h"
#include "dsda/hud.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/pause.h"
#include "dsda/settings.h"
#include "dsda/stretch.h"
#include "g_overflow.h"

// global heads up display controls

int hud_displayed;    //jff 2/23/98 turns heads-up display on/off

//
// Locally used constants, shortcuts.
//
#define HU_TITLEX 0
//jff 2/16/98 change 167 to ST_Y-1
// CPhipps - changed to ST_TY
// proff - changed to 200-ST_HEIGHT for stretching
#define HU_TITLEY ((200-ST_HEIGHT) - 1 - hu_font[0].height)

//jff 2/16/98 add coord text widget coordinates
// proff - changed to SCREENWIDTH to 320 for stretching
#define HU_COORDX (320 - 13*hu_font2['A'-HU_FONTSTART].width)
//jff 3/3/98 split coord widget into three lines in upper right of screen
#define HU_COORDXYZ_Y (1 * hu_font['A'-HU_FONTSTART].height + 1)
#define HU_COORDX_Y (0 + 0*hu_font['A'-HU_FONTSTART].height + HU_COORDXYZ_Y)
#define HU_COORDY_Y (1 + 1*hu_font['A'-HU_FONTSTART].height + HU_COORDXYZ_Y)
#define HU_COORDZ_Y (2 + 2*hu_font['A'-HU_FONTSTART].height + HU_COORDXYZ_Y)

#define HU_MAP_STAT_X (0)
#define HU_MAP_STAT_Y (1 * hu_font['A'-HU_FONTSTART].height + 1)
#define HU_MAP_MONSTERS_Y  (0 + 0*hu_font['A'-HU_FONTSTART].height + HU_MAP_STAT_Y)
#define HU_MAP_SECRETS_Y   (1 + 1*hu_font['A'-HU_FONTSTART].height + HU_MAP_STAT_Y)
#define HU_MAP_ITEMS_Y     (2 + 2*hu_font['A'-HU_FONTSTART].height + HU_MAP_STAT_Y)
#define HU_MAP_TIME_Y      (4 + 4*hu_font['A'-HU_FONTSTART].height + HU_MAP_STAT_Y)
#define HU_MAP_TOTALTIME_Y (5 + 5*hu_font['A'-HU_FONTSTART].height + HU_MAP_STAT_Y)

static player_t*  plr;

// font sets
patchnum_t hu_font[HU_FONTSIZE];
patchnum_t hu_font2[HU_FONTSIZE];
patchnum_t hu_msgbg[9];          //jff 2/26/98 add patches for message background

// widgets
static hu_textline_t  w_title;
static hu_stext_t     w_message;
static hu_textline_t  w_coordx; //jff 2/16/98 new coord widget for automap
static hu_textline_t  w_coordy; //jff 3/3/98 split coord widgets automap
static hu_textline_t  w_coordz; //jff 3/3/98 split coord widgets automap
static hu_mtext_t     w_rtext;  //jff 2/26/98 text message refresh widget

static hu_textline_t  w_map_monsters;  //e6y monsters widget for automap
static hu_textline_t  w_map_secrets;   //e6y secrets widgets automap
static hu_textline_t  w_map_items;     //e6y items widgets automap
static hu_textline_t  w_map_time;      //e6y level time widgets automap
static hu_textline_t  w_map_totaltime; //e6y total time widgets automap

static dboolean    always_off = false;
static dboolean    message_on;
static dboolean    message_list; //2/26/98 enable showing list of messages
dboolean           message_dontfuckwithme;
static dboolean    message_nottobefuckedwith;
static int         message_counter;
static int         yellow_message;

//jff 2/16/98 hud supported automap colors added
int hudcolor_titl;  // color range of automap level title
int hudcolor_xyco;  // color range of new coords on automap
int hudcolor_mapstat_title;
int hudcolor_mapstat_value;
int hudcolor_mapstat_time;
//jff 2/16/98 hud text colors, controls added
int hudcolor_mesg;  // color range of scrolling messages
int hud_msg_lines;  // number of message lines in window
//jff 2/26/98 hud text colors, controls added
int hudcolor_list;  // list of messages color
int hud_list_bgon;  // enable for solid window background for message list

//jff 2/16/98 initialization strings for ammo, health, armor widgets
static char hud_coordstrx[32];
static char hud_coordstry[32];
static char hud_coordstrz[32];
static char hud_ammostr[80];
static char hud_healthstr[80];
static char hud_armorstr[80];
static char hud_weapstr[80];
static char hud_keysstr[80];
static char hud_gkeysstr[80]; //jff 3/7/98 add support for graphic key display
static char hud_monsecstr[80];

extern int map_point_coordinates;
extern int map_level_stat;

static custom_message_t custom_message[MAX_MAXPLAYERS];
static custom_message_t *custom_message_p;
void HU_init_crosshair(void);

static void HU_SetLumpTrans(const char *name)
{
  int lump = W_CheckNumForName(name);
  if (lump > 0)
  {
    lumpinfo[lump].flags |= LUMP_CM2RGB;
  }
}

void HU_AddCharToTitle(char s)
{
  HUlib_addCharToTextLine(&w_title, s);
}

//
// HU_Init()
//
// Initialize the heads-up display, text that overwrites the primary display
//
// Passed nothing, returns nothing
//
void HU_Init(void)
{

  int   i;
  int   j;
  char  buffer[9];

  // load the heads-up font
  j = HU_FONTSTART;
  for (i = 0; i < HU_FONTSIZE - 1; i++, j++)
  {
    sprintf(buffer, "DIG%.3d", j);
    R_SetPatchNum(&hu_font2[i], buffer);
  }

  j = HU_FONTSTART;
  for (i=0;i<HU_FONTSIZE;i++,j++)
  {
    if ('0'<=j && j<='9')
    {
      if (raven)
        sprintf(buffer, "FONTA%.2d",j - 32);
      else
        sprintf(buffer, "STCFN%.3d",j);
      R_SetPatchNum(&hu_font[i], buffer);
    }
    else if ('A'<=j && j<='Z')
    {
      if (raven)
        sprintf(buffer, "FONTA%.2d",j - 32);
      else
        sprintf(buffer, "STCFN%.3d",j);
      R_SetPatchNum(&hu_font[i], buffer);
    }
    else if (!raven && j < 97)
    {
      sprintf(buffer, "STCFN%.3d",j);
      R_SetPatchNum(&hu_font[i], buffer);
      //jff 2/23/98 make all font chars defined, useful or not
    }
    else if (raven && j < 91)
    {
      sprintf(buffer, "FONTA%.2d", j - 32);
      R_SetPatchNum(&hu_font[i], buffer);
      //jff 2/23/98 make all font chars defined, useful or not
    }
    else
    {
      hu_font[i] = hu_font[0]; //jff 2/16/98 account for gap
    }
  }

  if (!raven)
  {
    // these patches require cm to rgb translation
    for (i = 33; i < 96; i++)
    {
      sprintf(buffer, "STCFN%.3d", i);
      HU_SetLumpTrans(buffer);
    }
    for (i = 0; i < 10; i++)
    {
      sprintf(buffer, "STTNUM%d", i);
      HU_SetLumpTrans(buffer);
    }
    HU_SetLumpTrans("STTPRCNT");
    HU_SetLumpTrans("STTMINUS");
  }

  // CPhipps - load patches for message background
  for (i=0; i<9; i++) {
    sprintf(buffer, "BOX%c%c", "UCL"[i/3], "LCR"[i%3]);
    R_SetPatchNum(&hu_msgbg[i], buffer);
  }
}

//
// HU_Start(void)
//
// Create and initialize the heads-up widgets, software machines to
// maintain, update, and display information over the primary display
//
// This routine must be called after any change to the heads up configuration
// in order for the changes to take effect in the actual displays
//
// Passed nothing, returns nothing
//
void HU_Start(void)
{
  int   i;
  const char* s; /* cph - const */

  plr = &players[displayplayer];        // killough 3/7/98
  custom_message_p = &custom_message[displayplayer];
  message_on = false;
  message_dontfuckwithme = false;
  message_nottobefuckedwith = false;
  yellow_message = false;

  // create the message widget
  // messages to player in upper-left of screen
  HUlib_initSText
  (
    &w_message,
    HU_MSGX,
    HU_MSGY,
    HU_MSGHEIGHT,
    hu_font,
    HU_FONTSTART,
    hudcolor_mesg,
    VPT_ALIGN_LEFT_TOP,
    &message_on
  );

  //jff 2/16/98 added some HUD widgets
  // create the map title widget - map title display in lower left of automap
  HUlib_initTextLine
  (
    &w_title,
    raven ? 20 : HU_TITLEX,
    raven ? heretic ? 145 : 144 : HU_TITLEY,
    hu_font,
    HU_FONTSTART,
    hudcolor_titl,
    VPT_ALIGN_LEFT_BOTTOM
  );

  // create the hud text refresh widget
  // scrolling display of last hud_msg_lines messages received
  if (hud_msg_lines>HU_MAXMESSAGES)
    hud_msg_lines=HU_MAXMESSAGES;
  //jff 4/21/98 if setup has disabled message list while active, turn it off
  message_list = hud_msg_lines > 1; //jff 8/8/98 initialize both ways
  //jff 2/26/98 add the text refresh widget initialization
  HUlib_initMText
  (
    &w_rtext,
    0,
    0,
    320,
//    SCREENWIDTH,
    (hud_msg_lines+2)*HU_REFRESHSPACING,
    hu_font,
    HU_FONTSTART,
    hudcolor_list,
    hu_msgbg,
    VPT_ALIGN_LEFT_TOP,
    &message_list
  );

  dsda_HUTitle(&s);

  while (*s)
    HU_AddCharToTitle(*(s++));

  // create the automaps coordinate widget
  // jff 3/3/98 split coord widget into three lines: x,y,z
  // jff 2/16/98 added
  HUlib_initTextLine
  (
    &w_coordx,
    HU_COORDX,
    HU_COORDX_Y,
    hu_font,
    HU_FONTSTART,
    hudcolor_xyco,
    VPT_ALIGN_RIGHT_TOP
  );
  HUlib_initTextLine
  (
    &w_coordy,
    HU_COORDX,
    HU_COORDY_Y,
    hu_font,
    HU_FONTSTART,
    hudcolor_xyco,
    VPT_ALIGN_RIGHT_TOP
  );
  HUlib_initTextLine
  (
    &w_coordz,
    HU_COORDX,
    HU_COORDZ_Y,
    hu_font,
    HU_FONTSTART,
    hudcolor_xyco,
    VPT_ALIGN_RIGHT_TOP
  );
//e6y
  HUlib_initTextLine
  (
    &w_map_monsters,
    HU_MAP_STAT_X,
    HU_MAP_MONSTERS_Y,
    hu_font,
    HU_FONTSTART,
    hudcolor_mapstat_title,
    VPT_ALIGN_LEFT_TOP
  );
  HUlib_initTextLine
  (
    &w_map_secrets,
    HU_MAP_STAT_X,
    HU_MAP_SECRETS_Y,
    hu_font,
    HU_FONTSTART,
    hudcolor_mapstat_title,
    VPT_ALIGN_LEFT_TOP
  );
  HUlib_initTextLine
  (
    &w_map_items,
    HU_MAP_STAT_X,
    HU_MAP_ITEMS_Y,
    hu_font,
    HU_FONTSTART,
    hudcolor_mapstat_title,
    VPT_ALIGN_LEFT_TOP
  );
  HUlib_initTextLine
  (
    &w_map_time,
    HU_MAP_STAT_X,
    HU_MAP_TIME_Y,
    hu_font,
    HU_FONTSTART,
    hudcolor_mapstat_time,
    VPT_ALIGN_LEFT_TOP
  );
  HUlib_initTextLine
  (
    &w_map_totaltime,
    HU_MAP_STAT_X,
    HU_MAP_TOTALTIME_Y,
    hu_font,
    HU_FONTSTART,
    hudcolor_mapstat_time,
    VPT_ALIGN_LEFT_TOP
  );
  HUlib_initTextLine
  (
    &w_hudadd,
    0, 0,
    hu_font2,
    HU_FONTSTART,
    CR_GRAY,
    VPT_NONE
  );
  HUlib_initTextLine
  (
    &w_centermsg,
    HU_CENTERMSGX,
    HU_CENTERMSGY,
    hu_font,
    HU_FONTSTART,
    hudcolor_titl,
    VPT_STRETCH
  );
  HUlib_initTextLine
  (
    &w_precache,
    16,
    186,
    hu_font,
    HU_FONTSTART,
    CR_RED,
    VPT_ALIGN_LEFT_BOTTOM
  );
  strcpy(hud_add,"");
  s = hud_add;
  while (*s)
    HUlib_addCharToTextLine(&w_hudadd, *(s++));

  HU_init_crosshair();

  dsda_InitHud(hu_font2);
}

int HU_GetHealthColor(int health, int def)
{
  int result;

  if (health < health_red)
    result = CR_RED;
  else if (health < health_yellow)
    result = CR_GOLD;
  else if (health <= health_green)
    result = CR_GREEN;
  else
    result = def;

  return result;
}

const char *crosshair_nam[HU_CROSSHAIRS] =
  { NULL, "CROSS1", "CROSS2", "CROSS3", "CROSS4", "CROSS5", "CROSS6", "CROSS7" };
const char *crosshair_str[HU_CROSSHAIRS] =
  { "none", "cross", "angle", "dot", "small", "slim", "tiny", "big" };
crosshair_t crosshair;

void HU_init_crosshair(void)
{
  if (!hudadd_crosshair || !crosshair_nam[hudadd_crosshair])
    return;

  crosshair.lump = W_CheckNumForNameInternal(crosshair_nam[hudadd_crosshair]);
  if (crosshair.lump == -1)
    return;

  crosshair.w = R_NumPatchWidth(crosshair.lump);
  crosshair.h = R_NumPatchHeight(crosshair.lump);

  crosshair.flags = VPT_TRANS;
  if (hudadd_crosshair_scale)
    crosshair.flags |= VPT_STRETCH;
}

void SetCrosshairTarget(void)
{
  crosshair.target_screen_x = 0.0f;
  crosshair.target_screen_y = 0.0f;

  if (dsda_CrosshairLockTarget() && crosshair.target_sprite >= 0)
  {
    float x, y, z;
    float winx, winy, winz;

    x = -(float)crosshair.target_x / MAP_SCALE;
    z =  (float)crosshair.target_y / MAP_SCALE;
    y =  (float)crosshair.target_z / MAP_SCALE;

    if (R_Project(x, y, z, &winx, &winy, &winz))
    {
      int top, bottom, h;
      stretch_param_t *params = dsda_StretchParams(crosshair.flags);

      if (V_IsSoftwareMode())
      {
        winy += (float)(viewheight/2 - centery);
      }

      top = SCREENHEIGHT - viewwindowy;
      h = crosshair.h;
      if (hudadd_crosshair_scale)
      {
        h = h * params->video->height / 200;
      }
      bottom = top - viewheight + h;
      winy = BETWEEN(bottom, top, winy);

      if (!hudadd_crosshair_scale)
      {
        crosshair.target_screen_x = winx - (crosshair.w / 2);
        crosshair.target_screen_y = SCREENHEIGHT - winy - (crosshair.h / 2);
      }
      else
      {
        crosshair.target_screen_x = (winx - params->deltax1) * 320.0f / params->video->width - (crosshair.w / 2);
        crosshair.target_screen_y = 200 - (winy - params->deltay1) * 200.0f / params->video->height - (crosshair.h / 2);
      }
    }
  }
}

void HU_draw_crosshair(void)
{
  int cm;

  crosshair.target_sprite = -1;

  if (
    !crosshair_nam[hudadd_crosshair] ||
    crosshair.lump == -1 ||
    automapmode & am_active ||
    menuactive ||
    dsda_Paused()
  )
  {
    return;
  }

  if (hudadd_crosshair_health)
    cm = HU_GetHealthColor(plr->health, CR_BLUE2);
  else
    cm = hudadd_crosshair_color;

  if (dsda_CrosshairTarget() || dsda_CrosshairLockTarget())
  {
    fixed_t slope;
    angle_t an = plr->mo->angle;

    // intercepts overflow guard
    overflows_enabled = false;
    slope = P_AimLineAttack(plr->mo, an, 16*64*FRACUNIT, 0);
    if (plr->readyweapon == wp_missile || plr->readyweapon == wp_plasma || plr->readyweapon == wp_bfg)
    {
      if (!linetarget)
        slope = P_AimLineAttack(plr->mo, an += 1<<26, 16*64*FRACUNIT, 0);
      if (!linetarget)
        slope = P_AimLineAttack(plr->mo, an -= 2<<26, 16*64*FRACUNIT, 0);
    }
    overflows_enabled = true;

    if (linetarget && !(linetarget->flags & MF_SHADOW))
    {
      crosshair.target_x = linetarget->x;
      crosshair.target_y = linetarget->y;
      crosshair.target_z = linetarget->z;
      crosshair.target_z += linetarget->height / 2 + linetarget->height / 8;
      crosshair.target_sprite = linetarget->sprite;

      if (dsda_CrosshairTarget())
        cm = hudadd_crosshair_target_color;
    }
  }

  SetCrosshairTarget();

  if (crosshair.target_screen_x != 0)
  {
    float x = crosshair.target_screen_x;
    float y = crosshair.target_screen_y;
    V_DrawNumPatchPrecise(x, y, 0, crosshair.lump, cm, crosshair.flags);
  }
  else
  {
    int x, y, st_height;

    if (!hudadd_crosshair_scale)
    {
      st_height = (R_PartialView() ? ST_SCALED_HEIGHT : 0);
      x = (SCREENWIDTH - crosshair.w) / 2;
      y = (SCREENHEIGHT - st_height - crosshair.h) / 2;
    }
    else
    {
      st_height = (R_PartialView() ? ST_HEIGHT : 0);
      x = (320 - crosshair.w) / 2;
      y = (200 - st_height - crosshair.h) / 2;
    }

    V_DrawNumPatch(x, y, 0, crosshair.lump, cm, crosshair.flags);
  }
}

//
// HU_Drawer()
//
// Draw all the pieces of the heads-up display
//
// Passed nothing, returns nothing
//
void HU_Drawer(void)
{
  char *s;
  player_t *plr;
  //jff 3/4/98 speed update up for slow systems
  //e6y: speed update for uncapped framerate
  static dboolean needupdate = false;
  if (realframe) needupdate = !needupdate;

  // don't draw anything if there's a fullscreen menu up
  if (menuactive == mnact_full)
    return;

  plr = &players[displayplayer];         // killough 3/7/98
  // draw the automap widgets if automap is displayed
  if (automapmode & am_active)
  {
    // Hide title if automap in overlay mode and adv / ex hud is active
    if (!(automapmode & am_overlay) || (R_PartialView() && !dsda_ExHud()))
    {
      // map title
      HUlib_drawTextLine(&w_title, false);
    }

    //jff 2/16/98 output new coord display
    // x-coord
    if (dsda_MapPointCoordinates())
    {

      //e6y: speedup
      if (!realframe)
      {
        HUlib_drawTextLine(&w_coordx, false);
        HUlib_drawTextLine(&w_coordy, false);
        HUlib_drawTextLine(&w_coordz, false);
      }
      else
      {
        sprintf(hud_coordstrx,"X: %-5d", (plr->mo->x)>>FRACBITS);
        HUlib_clearTextLine(&w_coordx);
        s = hud_coordstrx;
        while (*s)
          HUlib_addCharToTextLine(&w_coordx, *(s++));
        HUlib_drawTextLine(&w_coordx, false);

        //jff 3/3/98 split coord display into x,y,z lines
        // y-coord
        sprintf(hud_coordstry,"Y: %-5d", (plr->mo->y)>>FRACBITS);
        HUlib_clearTextLine(&w_coordy);
        s = hud_coordstry;
        while (*s)
          HUlib_addCharToTextLine(&w_coordy, *(s++));
        HUlib_drawTextLine(&w_coordy, false);

        //jff 3/3/98 split coord display into x,y,z lines
        //jff 2/22/98 added z
        // z-coord
        sprintf(hud_coordstrz,"Z: %-5d", (plr->mo->z)>>FRACBITS);
        HUlib_clearTextLine(&w_coordz);
        s = hud_coordstrz;
        while (*s)
          HUlib_addCharToTextLine(&w_coordz, *(s++));
        HUlib_drawTextLine(&w_coordz, false);
      }
    }

    if (map_level_stat)
    {
      static char str[32];
      int time = leveltime / TICRATE;
      int ttime = (totalleveltimes + leveltime) / TICRATE;

      if (hexen)
        ttime = players[consoleplayer].worldTimer / TICRATE;

      sprintf(str, "Monsters: \x1b%c%d/%d", HUlib_Color(hudcolor_mapstat_value),
        players[consoleplayer].killcount - players[consoleplayer].maxkilldiscount,
        totalkills);
      HUlib_clearTextLine(&w_map_monsters);
      s = str;
      while (*s)
        HUlib_addCharToTextLine(&w_map_monsters, *(s++));
      HUlib_drawTextLine(&w_map_monsters, false);

      sprintf(str, "Secrets: \x1b%c%d/%d", HUlib_Color(hudcolor_mapstat_value),
        players[consoleplayer].secretcount, totalsecret);
      HUlib_clearTextLine(&w_map_secrets);
      s = str;
      while (*s)
        HUlib_addCharToTextLine(&w_map_secrets, *(s++));
      HUlib_drawTextLine(&w_map_secrets, false);

      sprintf(str, "Items: \x1b%c%d/%d", HUlib_Color(hudcolor_mapstat_value),
        players[consoleplayer].itemcount, totalitems);
      HUlib_clearTextLine(&w_map_items);
      s = str;
      while (*s)
        HUlib_addCharToTextLine(&w_map_items, *(s++));
      HUlib_drawTextLine(&w_map_items, false);

      sprintf(str, "%02d:%02d:%02d", time/3600, (time%3600)/60, time%60);
      HUlib_clearTextLine(&w_map_time);
      s = str;
      while (*s)
        HUlib_addCharToTextLine(&w_map_time, *(s++));
      HUlib_drawTextLine(&w_map_time, false);

      if (hexen || totalleveltimes > 0)
      {
        sprintf(str, "%02d:%02d:%02d", ttime/3600, (ttime%3600)/60, ttime%60);
        HUlib_clearTextLine(&w_map_totaltime);
        s = str;
        while (*s)
          HUlib_addCharToTextLine(&w_map_totaltime, *(s++));
        HUlib_drawTextLine(&w_map_totaltime, false);
      }
    }
  }

  //jff 3/4/98 display last to give priority
  HU_Erase(); // jff 4/24/98 Erase current lines before drawing current
              // needed when screen not fullsize

  if (hudadd_crosshair)
    HU_draw_crosshair();

  //jff 4/21/98 if setup has disabled message list while active, turn it off
  if (hud_msg_lines<=1)
    message_list = false;

  // if the message review not enabled, show the standard message widget
  if (!message_list)
    HUlib_drawSText(&w_message);

  //e6y
  if (custom_message_p->ticks > 0)
    HUlib_drawTextLine(&w_centermsg, false);

  // if the message review is enabled show the scrolling message review
  if (hud_msg_lines>1 && message_list)
    HUlib_drawMText(&w_rtext);

  dsda_DrawHud();
}

//
// HU_Erase()
//
// Erase hud display lines that can be trashed by small screen display
//
// Passed nothing, returns nothing
//
void HU_Erase(void)
{
  // erase the message display or the message review display
  if (!message_list)
    HUlib_eraseSText(&w_message);
  else
    HUlib_eraseMText(&w_rtext);

  //e6y
  if (custom_message_p->ticks > 0)
    HUlib_eraseTextLine(&w_centermsg);

  // erase the automap title
  HUlib_eraseTextLine(&w_title);

  dsda_EraseHud();
}

//
// HU_Ticker()
//
// Update the hud displays once per frame
//
// Passed nothing, returns nothing
//
void HU_Ticker(void)
{
  int i, rc;
  char c;

  // tick down message counter if message is up
  if (message_counter && !--message_counter)
  {
    message_on = false;
    message_nottobefuckedwith = false;
    yellow_message = false;
  }

  // if messages on, or "Messages Off" is being displayed
  // this allows the notification of turning messages off to be seen
  if (dsda_ShowMessages() || message_dontfuckwithme)
  {
    // display message if necessary
    if ((plr->message && !message_nottobefuckedwith)
        || (plr->message && message_dontfuckwithme))
    {
      //post the message to the message widget
      HUlib_addMessageToSText(&w_message, 0, plr->message);
      //jff 2/26/98 add message to refresh text widget too
      HUlib_addMessageToMText(&w_rtext, 0, plr->message);

      // clear the message to avoid posting multiple times
      plr->message = 0;
      // note a message is displayed
      message_on = true;
      // start the message persistence counter
      message_counter = HU_MSGTIMEOUT;
      // transfer "Messages Off" exception to the "being displayed" variable
      message_nottobefuckedwith = message_dontfuckwithme;
      // clear the flag that "Messages Off" is being posted
      message_dontfuckwithme = 0;

      yellow_message = plr->yellowMessage;
      // hexen_note: use FONTAY_S for yellow messages (new font, y_message, etc)
    }
  }

  // centered messages
  for (i = 0; i < g_maxplayers; i++)
  {
    if (custom_message[i].ticks > 0)
      custom_message[i].ticks--;
  }
  if (custom_message_p->msg)
  {
    const char *s = custom_message_p->msg;
    HUlib_clearTextLine(&w_centermsg);
    while (*s)
    {
      HUlib_addCharToTextLine(&w_centermsg, *(s++));
    }
    HUlib_setTextXCenter(&w_centermsg);
    w_centermsg.cm = custom_message_p->cm;
    custom_message_p->msg = NULL;

    if (custom_message_p->sfx > 0 && custom_message_p->sfx < num_sfx)
    {
      S_StartSound(NULL, custom_message_p->sfx);
    }
  }

  dsda_UpdateHud();
}

//
// HU_Responder()
//
// Responds to input events that affect the heads up displays
//
// Passed the event to respond to, returns true if the event was handled
//
dboolean HU_Responder(event_t *ev)
{
  if (dsda_InputActivated(dsda_input_repeat_message)) // phares
  {
    if (hud_msg_lines>1)  // it posts multi-line messages that will trash
    {
      if (message_list) HU_Erase(); //jff 4/28/98 erase behind messages
      message_list = !message_list; //jff 2/26/98 toggle list of messages
    }

    if (!message_list)              // if not message list, refresh message
    {
      message_on = true;
      message_counter = HU_MSGTIMEOUT;
    }

    return true;
  }

  return false;
}

void T_ShowMessage (message_thinker_t* message)
{
  if (--message->delay > 0)
    return;

  SetCustomMessage(message->plr, message->msg.msg, 0,
    message->msg.ticks, message->msg.cm, message->msg.sfx);

  P_RemoveThinker(&message->thinker); // unlink and free
}

int SetCustomMessage(int plr, const char *msg, int delay, int ticks, int cm, int sfx)
{
  custom_message_t item;

  if (plr < 0 || plr >= g_maxplayers || !msg || ticks < 0 ||
      sfx < 0 || sfx >= num_sfx || cm < 0 || cm >= CR_LIMIT)
  {
    return false;
  }

  item.msg = msg;
  item.ticks = ticks;
  item.cm = cm;
  item.sfx = sfx;

  if (delay <= 0)
  {
    custom_message[plr] = item;
  }
  else
  {
    message_thinker_t *message = Z_CallocLevel(1, sizeof(*message));
    message->thinker.function = T_ShowMessage;
    message->delay = delay;
    message->plr = plr;

    message->msg = item;

    P_AddThinker(&message->thinker);
  }

  return true;
}

void ClearMessage(void)
{
  message_counter = 0;
  yellow_message = false;
}
