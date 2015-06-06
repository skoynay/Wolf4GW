// WL_PLAY.C

#include <i86.h>
#pragma hdrstop

extern byte scanbuffer[16];
extern int curscanoffs;

extern boolean NumLockPanic;

extern boolean PushwallSteps;

/*
=============================================================================

                                                 LOCAL CONSTANTS

=============================================================================
*/

#define sc_Question     0x35

/*
=============================================================================

                                                 GLOBAL VARIABLES

=============================================================================
*/

extern union REGS regs;
boolean         madenoise;                                      // true when shooting or screaming

exit_t          playstate;

musicnames      lastmusicchunk = (musicnames) -1;

int                     DebugOk;

objtype         objlist[MAXACTORS];
objtype *newobj,*obj,*player,*lastobj,
                        *objfreelist,*killerobj;

boolean                 noclip,ammocheat;
int             godmode,singlestep,extravbls=1;            // to remove flicker (gray stuff at the bottom)

byte            tilemap[MAPSIZE][MAPSIZE];      // wall values only
byte            spotvis[MAPSIZE][MAPSIZE];
objtype         *actorat[MAPSIZE][MAPSIZE];

//
// replacing refresh manager
//
unsigned tics;

//
// control info
//
boolean         mouseenabled,joystickenabled,joypadenabled,joystickprogressive;
int                     joystickport;
int                     dirscan[4] = {sc_UpArrow,sc_RightArrow,sc_DownArrow,sc_LeftArrow};
int                     buttonscan[NUMBUTTONS] =
                        {sc_Control,sc_Alt,sc_RShift,sc_Space,sc_1,sc_2,sc_3,sc_4};
int                     buttonmouse[4]={bt_attack,bt_strafe,bt_use,bt_nobutton};
int                     buttonjoy[4]={bt_attack,bt_strafe,bt_use,bt_run};

int                     viewsize;

boolean         buttonheld[NUMBUTTONS];

boolean         demorecord,demoplayback;
char            *demoptr, *lastdemoptr;
memptr          demobuffer;

//
// curent user input
//
int                     controlx,controly;              // range from -100 to 100 per tic
boolean         buttonstate[NUMBUTTONS];

int lastgamemusicoffset=0;


//===========================================================================


void    CenterWindow(word w,word h);
void    InitObjList (void);
void    RemoveObj (objtype *gone);
void    PollControls (void);
int     StopMusic(void);
void    StartMusic(void);
void    ContinueMusic(int offs);
void    PlayLoop (void);

/*
=============================================================================

                                                 LOCAL VARIABLES

=============================================================================
*/


objtype dummyobj;

//
// LIST OF SONGS FOR EACH VERSION
//
int songs[]=
{
#ifndef SPEAR
 //
 // Episode One
 //
 GETTHEM_MUS,
 SEARCHN_MUS,
 POW_MUS,
 SUSPENSE_MUS,
 GETTHEM_MUS,
 SEARCHN_MUS,
 POW_MUS,
 SUSPENSE_MUS,

 WARMARCH_MUS,  // Boss level
 CORNER_MUS,    // Secret level

 //
 // Episode Two
 //
 NAZI_OMI_MUS,
 PREGNANT_MUS,
 GOINGAFT_MUS,
 HEADACHE_MUS,
 NAZI_OMI_MUS,
 PREGNANT_MUS,
 HEADACHE_MUS,
 GOINGAFT_MUS,

 WARMARCH_MUS,  // Boss level
 DUNGEON_MUS,   // Secret level

 //
 // Episode Three
 //
 INTROCW3_MUS,
 NAZI_RAP_MUS,
 TWELFTH_MUS,
 ZEROHOUR_MUS,
 INTROCW3_MUS,
 NAZI_RAP_MUS,
 TWELFTH_MUS,
 ZEROHOUR_MUS,

 ULTIMATE_MUS,  // Boss level
 PACMAN_MUS,    // Secret level

 //
 // Episode Four
 //
 GETTHEM_MUS,
 SEARCHN_MUS,
 POW_MUS,
 SUSPENSE_MUS,
 GETTHEM_MUS,
 SEARCHN_MUS,
 POW_MUS,
 SUSPENSE_MUS,

 WARMARCH_MUS,  // Boss level
 CORNER_MUS,    // Secret level

 //
 // Episode Five
 //
 NAZI_OMI_MUS,
 PREGNANT_MUS,
 GOINGAFT_MUS,
 HEADACHE_MUS,
 NAZI_OMI_MUS,
 PREGNANT_MUS,
 HEADACHE_MUS,
 GOINGAFT_MUS,

 WARMARCH_MUS,  // Boss level
 DUNGEON_MUS,   // Secret level

 //
 // Episode Six
 //
 INTROCW3_MUS,
 NAZI_RAP_MUS,
 TWELFTH_MUS,
 ZEROHOUR_MUS,
 INTROCW3_MUS,
 NAZI_RAP_MUS,
 TWELFTH_MUS,
 ZEROHOUR_MUS,

 ULTIMATE_MUS,  // Boss level
 FUNKYOU_MUS            // Secret level
#else

 //////////////////////////////////////////////////////////////
 //
 // SPEAR OF DESTINY TRACKS
 //
 //////////////////////////////////////////////////////////////
 XTIPTOE_MUS,
 XFUNKIE_MUS,
 XDEATH_MUS,
 XGETYOU_MUS,           // DON'T KNOW
 ULTIMATE_MUS,  // Trans Gr�sse

 DUNGEON_MUS,
 GOINGAFT_MUS,
 POW_MUS,
 TWELFTH_MUS,
 ULTIMATE_MUS,  // Barnacle Wilhelm BOSS

 NAZI_OMI_MUS,
 GETTHEM_MUS,
 SUSPENSE_MUS,
 SEARCHN_MUS,
 ZEROHOUR_MUS,
 ULTIMATE_MUS,  // Super Mutant BOSS

 XPUTIT_MUS,
 ULTIMATE_MUS,  // Death Knight BOSS

 XJAZNAZI_MUS,  // Secret level
 XFUNKIE_MUS,   // Secret level (DON'T KNOW)

 XEVIL_MUS              // Angel of Death BOSS

#endif
};


/*
=============================================================================

                                                  USER CONTROL

=============================================================================
*/


#define BASEMOVE                35
#define RUNMOVE                 70
#define BASETURN                35
#define RUNTURN                 70

#define JOYSCALE                2

/*
===================
=
= PollKeyboardButtons
=
===================
*/

void PollKeyboardButtons (void)
{
        int             i;

        for (i=0;i<NUMBUTTONS;i++)
                if (Keyboard[buttonscan[i]])
                        buttonstate[i] = true;
}


/*
===================
=
= PollMouseButtons
=
===================
*/

void PollMouseButtons (void)
{
        int     buttons;

        buttons = IN_MouseButtons ();

        if (buttons&1)
                buttonstate[buttonmouse[0]] = true;
        if (buttons&2)
                buttonstate[buttonmouse[1]] = true;
        if (buttons&4)
                buttonstate[buttonmouse[2]] = true;
}



/*
===================
=
= PollJoystickButtons
=
===================
*/

void PollJoystickButtons (void)
{
        int     buttons;

        buttons = IN_JoyButtons ();

        if (joystickport && !joypadenabled)
        {
                if (buttons&4)
                        buttonstate[buttonjoy[0]] = true;
                if (buttons&8)
                        buttonstate[buttonjoy[1]] = true;
        }
        else
        {
                if (buttons&1)
                        buttonstate[buttonjoy[0]] = true;
                if (buttons&2)
                        buttonstate[buttonjoy[1]] = true;
                if (joypadenabled)
                {
                        if (buttons&4)
                                buttonstate[buttonjoy[2]] = true;
                        if (buttons&8)
                                buttonstate[buttonjoy[3]] = true;
                }
        }
}


/*
===================
=
= PollKeyboardMove
=
===================
*/

void PollKeyboardMove (void)
{
        if (buttonstate[bt_run])
        {
                if (Keyboard[dirscan[di_north]])
                        controly -= RUNMOVE*tics;
                if (Keyboard[dirscan[di_south]])
                        controly += RUNMOVE*tics;
                if (Keyboard[dirscan[di_west]])
                        controlx -= RUNMOVE*tics;
                if (Keyboard[dirscan[di_east]])
                        controlx += RUNMOVE*tics;
        }
        else
        {
                if (Keyboard[dirscan[di_north]])
                        controly -= BASEMOVE*tics;
                if (Keyboard[dirscan[di_south]])
                        controly += BASEMOVE*tics;
                if (Keyboard[dirscan[di_west]])
                        controlx -= BASEMOVE*tics;
                if (Keyboard[dirscan[di_east]])
                        controlx += BASEMOVE*tics;
        }
}


/*
===================
=
= PollMouseMove
=
===================
*/

void PollMouseMove (void)
{
//      short mousexmove,mouseymove;
        int mousexmove,mouseymove;

        Mouse(MDelta);
        mousexmove = (short)_CX;
        mouseymove = (short)_DX;

        controlx += mousexmove*10/(13-mouseadjustment);
        controly += mouseymove*20/(13-mouseadjustment);
}



/*
===================
=
= PollJoystickMove
=
===================
*/

void PollJoystickMove (void)
{
        int     joyx,joyy;

        INL_GetJoyDelta(joystickport,&joyx,&joyy);

        if (joystickprogressive)
        {
                if (joyx > 64)
                        controlx += (joyx-64)*JOYSCALE*tics;
                else if (joyx < -64)
                        controlx -= (-joyx-64)*JOYSCALE*tics;
                if (joyy > 64)
                        controlx += (joyy-64)*JOYSCALE*tics;
                else if (joyy < -64)
                        controly -= (-joyy-64)*JOYSCALE*tics;
        }
        else if (buttonstate[bt_run])
        {
                if (joyx > 64)
                        controlx += RUNMOVE*tics;
                else if (joyx < -64)
                        controlx -= RUNMOVE*tics;
                if (joyy > 64)
                        controly += RUNMOVE*tics;
                else if (joyy < -64)
                        controly -= RUNMOVE*tics;
        }
        else
        {
                if (joyx > 64)
                        controlx += BASEMOVE*tics;
                else if (joyx < -64)
                        controlx -= BASEMOVE*tics;
                if (joyy > 64)
                        controly += BASEMOVE*tics;
                else if (joyy < -64)
                        controly -= BASEMOVE*tics;
        }
}


/*
===================
=
= PollControls
=
= Gets user or demo input, call once each frame
=
= controlx              set between -100 and 100 per tic
= controly
= buttonheld[]  the state of the buttons LAST frame
= buttonstate[] the state of the buttons THIS frame
=
===================
*/

void PollControls (void)
{
        int             max,min,i;
        byte    buttonbits;

//
// get timing info for last frame
//
        if (demoplayback)
        {
                while (TimeCount<lasttimecount+DEMOTICS)
                ;
                TimeCount = lasttimecount + DEMOTICS;
                lasttimecount += DEMOTICS;
                tics = DEMOTICS;
        }
        else if (demorecord)                    // demo recording and playback needs
        {                                                               // to be constant
//
// take DEMOTICS or more tics, and modify Timecount to reflect time taken
//
                while (TimeCount<lasttimecount+DEMOTICS)
                ;
                TimeCount = lasttimecount + DEMOTICS;
                lasttimecount += DEMOTICS;
                tics = DEMOTICS;
        }
        else
                CalcTics ();

        controlx = 0;
        controly = 0;
        memcpy (buttonheld,buttonstate,sizeof(buttonstate));
        memset (buttonstate,0,sizeof(buttonstate));

        if (demoplayback)
        {
        //
        // read commands from demo buffer
        //
                buttonbits = *demoptr++;
                for (i=0;i<NUMBUTTONS;i++)
                {
                        buttonstate[i] = buttonbits&1;
                        buttonbits >>= 1;
                }

                controlx = *demoptr++;
                controly = *demoptr++;

                if (demoptr == lastdemoptr)
                        playstate = ex_completed;               // demo is done

                controlx *= (int)tics;
                controly *= (int)tics;

                return;
        }


//
// get button states
//
        PollKeyboardButtons ();

        if (mouseenabled)
                PollMouseButtons ();

        if (joystickenabled)
                PollJoystickButtons ();

//
// get movements
//
        PollKeyboardMove ();

        if (mouseenabled)
                PollMouseMove ();

        if (joystickenabled)
                PollJoystickMove ();

//
// bound movement to a maximum
//
        max = 100*tics;
        min = -max;
        if (controlx > max)
                controlx = max;
        else if (controlx < min)
                controlx = min;

        if (controly > max)
                controly = max;
        else if (controly < min)
                controly = min;

        if (demorecord)
        {
        //
        // save info out to demo buffer
        //
                controlx /= (int)tics;
                controly /= (int)tics;

                buttonbits = 0;

                for (i=NUMBUTTONS-1;i>=0;i--)
                {
                        buttonbits <<= 1;
                        if (buttonstate[i])
                                buttonbits |= 1;
                }

                *demoptr++ = buttonbits;
                *demoptr++ = controlx;
                *demoptr++ = controly;

                if (demoptr >= lastdemoptr-8)
                        playstate = ex_completed;
                else
                {
                        controlx *= (int)tics;
                        controly *= (int)tics;
                }
        }
}



//==========================================================================



///////////////////////////////////////////////////////////////////////////
//
//      CenterWindow() - Generates a window of a given width & height in the
//              middle of the screen
//
///////////////////////////////////////////////////////////////////////////
#define MAXX    320
#define MAXY    160

void    CenterWindow(word w,word h)
{
        FixOfs ();
        US_DrawWindow(((MAXX / 8) - w) / 2,((MAXY / 8) - h) / 2,w,h);
}

//===========================================================================


/*
=====================
=
= CheckKeys
=
=====================
*/

void CheckKeys (void)
{
        byte    scan;


        if (screenfaded || demoplayback)        // don't do anything with a faded screen
                return;

        scan = LastScan;


        #ifdef SPEAR
        //
        // SECRET CHEAT CODE: TAB-G-F10
        //
        if (Keyboard[sc_Tab] &&
                Keyboard[sc_G] &&
                Keyboard[sc_F10])
        {
                WindowH = 160;
                if (godmode)
                {
                        Message ("God mode OFF");
                        SD_PlaySound (NOBONUSSND);
                }
                else
                {
                        Message ("God mode ON");
                        SD_PlaySound (ENDBONUS2SND);
                }

                IN_Ack();
                godmode ^= 1;
                DrawAllPlayBorderSides ();
                IN_ClearKeysDown();
                return;
        }
        #endif


        //
        // SECRET CHEAT CODE: 'MLI'
        //
        if (Keyboard[sc_M] &&
                Keyboard[sc_L] &&
                Keyboard[sc_I])
        {
                gamestate.health = 100;
                gamestate.ammo = 99;
                gamestate.keys = 3;
                gamestate.score = 0;
                gamestate.TimeCount += 42000L;
                GiveWeapon (wp_chaingun);
                DrawWeapon();
                DrawHealth();
                DrawKeys();
                DrawAmmo();
                DrawScore();

                ClearMemory ();
                CA_CacheGrChunk (STARTFONT+1);
                ClearSplitVWB ();
                VW_ScreenToScreen (vdisp,vbuf,80,160);

                Message(STR_CHEATER1"\n"
                                STR_CHEATER2"\n\n"
                                STR_CHEATER3"\n"
                                STR_CHEATER4"\n"
                                STR_CHEATER5);

                UNCACHEGRCHUNK(STARTFONT+1);
                IN_ClearKeysDown();
                IN_Ack();

                if (viewsize < 17)
                        DrawAllPlayBorder ();
        }

        //
        // OPEN UP DEBUG KEYS
        //
#ifdef DEBUGKEYS
#ifndef SPEAR
        if (Keyboard[sc_BackSpace] &&
                Keyboard[sc_LShift] &&
                Keyboard[sc_Alt]/* &&
                MS_CheckParm("goobers")*/)
#else
        if (Keyboard[sc_BackSpace] &&
                Keyboard[sc_LShift] &&
                Keyboard[sc_Alt] &&
                MS_CheckParm("debugmode"))
#endif
        {
         ClearMemory ();
         CA_CacheGrChunk (STARTFONT+1);
         ClearSplitVWB ();
         VW_ScreenToScreen (vdisp,vbuf,80,160);

         Message("Debugging keys are\nnow available!");
         UNCACHEGRCHUNK(STARTFONT+1);
         IN_ClearKeysDown();
         IN_Ack();

         DrawAllPlayBorderSides ();
         DebugOk=1;
        }
#endif

        //
        // TRYING THE KEEN CHEAT CODE!
        //
        if (Keyboard[sc_B] &&
                Keyboard[sc_A] &&
                Keyboard[sc_T])
        {
         ClearMemory ();
         CA_CacheGrChunk (STARTFONT+1);
         ClearSplitVWB ();
         VW_ScreenToScreen (vdisp,vbuf,80,160);

         Message("Commander Keen is also\n"
                         "available from Apogee, but\n"
                         "then, you already know\n"
                         "that - right, Cheatmeister?!");

         UNCACHEGRCHUNK(STARTFONT+1);
         IN_ClearKeysDown();
         IN_Ack();

         if (viewsize < 18)
                DrawAllPlayBorder ();
        }

//
// pause key weirdness can't be checked as a scan code
//
        if (Paused)
        {
                byte *temp=vbuf;
                int oldTimeCount=TimeCount;
                vbuf=vdisp;
                LatchDrawPic (20-4,80-2*8,PAUSEDPIC);
                SD_MusicOff();
                IN_Ack();
                Paused = false;
                vbuf=temp;
                SD_MusicOn();
                if (MousePresent)
                        Mouse(MDelta);  // Clear accumulated mouse movement
                TimeCount=oldTimeCount;
                return;
        }

#ifdef DEBUGKEYS
        if (Keyboard[sc_Home])          // old Ripper debugcodes (press Home+key to activate) :D
        {
                if(Keyboard[sc_B])
                {
                        char str[80];
                        CA_CacheGrChunk (STARTFONT+1);
                        ClearSplitVWB ();
                        VW_ScreenToScreen (vdisp,vbuf,80,160);
                        for(int i=0;i<4;i++)
                        {
                                sprintf(str+i*12,"%.2X %.2X %.2X %.2X\n",scanbuffer[(curscanoffs+i*4+1)&15],
                                scanbuffer[(curscanoffs+i*4+2)&15],scanbuffer[(curscanoffs+i*4+3)&15],
                                scanbuffer[(curscanoffs+i*4+4)&15]);
                        }

                        Message(str);

                        UNCACHEGRCHUNK(STARTFONT+1);
                        IN_ClearKeysDown();
                        IN_Ack();

                        DrawAllPlayBorder ();
                }

                if(Keyboard[sc_S])
                {
                        extern int lastsoundstarted;
                        extern int lastdigiwhich;
                        extern int lastdigistart;
                        extern int lastdigisegstart;

                        char str[80];
                        CA_CacheGrChunk(STARTFONT+1);
                        ClearSplitVWB();
                        VW_ScreenToScreen(vdisp,vbuf,80,160);
                        sprintf(str,"lastsnd=%i\nlastwhich=%i\nlaststart=%i\nlastseg=%.8X",lastsoundstarted,lastdigiwhich,lastdigistart,lastdigisegstart);
                        Message(str);
                        UNCACHEGRCHUNK(STARTFONT+1);
                        IN_ClearKeysDown();
                        IN_Ack();
                        DrawAllPlayBorder();
                }               

                if(Keyboard[sc_D])
                {
                        extern byte *DMABuffer;

                        char str[80];
                        CA_CacheGrChunk(STARTFONT+1);
                        ClearSplitVWB();
                        VW_ScreenToScreen(vdisp,vbuf,80,160);
                        sprintf(str,"DMABuffer=%.8X",(int)DMABuffer);
                        Message(str);
                        UNCACHEGRCHUNK(STARTFONT+1);
                        IN_ClearKeysDown();
                        IN_Ack();
                        DrawAllPlayBorder();
                }
					 if(Keyboard[sc_P])
					 {
						 PushwallSteps = !PushwallSteps;
                   CA_CacheGrChunk(STARTFONT+1);
                   ClearSplitVWB();
                   VW_ScreenToScreen(vdisp,vbuf,80,160);
                   Message(PushwallSteps ? "Pushwall stepping enabled (p)" : "Pushwall stepping disabled");
						
                   UNCACHEGRCHUNK(STARTFONT+1);
                   IN_ClearKeysDown();
                   IN_Ack();
                   DrawAllPlayBorder();
					 }
        }
#endif


//
// F1-F7/ESC to enter control panel
//
        if (
#ifndef DEBCHECK
                scan == sc_F10 ||
#endif
                scan == sc_F9 ||
                scan == sc_F7 ||
                scan == sc_F8)                  // pop up quit dialog
        {
                short oldmapon=gamestate.mapon;
                short oldepisode=gamestate.episode;     
                ClearMemory ();
                ClearSplitVWB ();
                VW_ScreenToScreen (vdisp,vbuf,80,160);
                US_ControlPanel(scan);

                DrawAllPlayBorderSides ();

                SETFONTCOLOR(0,15);
                IN_ClearKeysDown();
                return;
        }

        if ( (scan >= sc_F1 && scan <= sc_F9) || scan == sc_Escape)
        {
                int lastoffs = StopMusic ();
                ClearMemory ();
                VW_FadeOut ();

                US_ControlPanel(scan);

                SETFONTCOLOR(0,15);
                IN_ClearKeysDown();
                DrawPlayScreen ();
                if (!startgame && !loadedgame)
                {
//                      VW_FadeIn ();   // bugfix
                        ContinueMusic (lastoffs);
                }
                if (loadedgame)
                        playstate = ex_abort;
                lasttimecount = TimeCount;
                if (MousePresent)
                        Mouse(MDelta);  // Clear accumulated mouse movement
                return;
        }

//
// TAB-? debug keys
//
#ifdef DEBUGKEYS
        if (Keyboard[sc_Tab] && DebugOk)
        {
                CA_CacheGrChunk (STARTFONT);
                fontnumber=0;
                SETFONTCOLOR(0,15);
                if (DebugKeys() && viewsize < 18)
                        DrawAllPlayBorder();    // dont let the blue borders flash
                
                if (MousePresent)
                        Mouse(MDelta);  // Clear accumulated mouse movement
                lasttimecount = TimeCount;
                return;
        }
#endif
}


//===========================================================================

/*
#############################################################################

                                  The objlist data structure

#############################################################################

objlist containt structures for every actor currently playing.  The structure
is accessed as a linked list starting at *player, ending when ob->next ==
NULL.  GetNewObj inserts a new object at the end of the list, meaning that
if an actor spawn another actor, the new one WILL get to think and react the
same frame.  RemoveObj unlinks the given object and returns it to the free
list, but does not damage the objects ->next pointer, so if the current object
removes itself, a linked list following loop can still safely get to the
next element.

<backwardly linked free list>

#############################################################################
*/


/*
=========================
=
= InitActorList
=
= Call to clear out the actor object lists returning them all to the free
= list.  Allocates a special spot for the player.
=
=========================
*/

int     objcount;

void InitActorList (void)
{
        int     i;

//
// init the actor lists
//
        for (i=0;i<MAXACTORS;i++)
        {
                objlist[i].prev = &objlist[i+1];
                objlist[i].next = NULL;
        }

        objlist[MAXACTORS-1].prev = NULL;

        objfreelist = &objlist[0];
        lastobj = NULL;

        objcount = 0;

//
// give the player the first free spots
//
        GetNewActor ();
        player = newobj;

}

//===========================================================================

/*
=========================
=
= GetNewActor
=
= Sets the global variable new to point to a free spot in objlist.
= The free spot is inserted at the end of the liked list
=
= When the object list is full, the caller can either have it bomb out ot
= return a dummy object pointer that will never get used
=
=========================
*/

void GetNewActor (void)
{
        if (!objfreelist)
                Quit ("GetNewActor: No free spots in objlist!");

        newobj = objfreelist;
        objfreelist = newobj->prev;
        memset (newobj,0,sizeof(*newobj));

        if (lastobj)
                lastobj->next = newobj;
        newobj->prev = lastobj; // new->next is allready NULL from memset

        newobj->active = ac_no;
        lastobj = newobj;

        objcount++;
}

//===========================================================================

/*
=========================
=
= RemoveObj
=
= Add the given object back into the free list, and unlink it from it's
= neighbors
=
=========================
*/

void RemoveObj (objtype *gone)
{
        if (gone == player)
                Quit ("RemoveObj: Tried to remove the player!");

        gone->state = NULL;

//
// fix the next object's back link
//
        if (gone == lastobj)
                lastobj = (objtype *)gone->prev;
        else
                gone->next->prev = gone->prev;

//
// fix the previous object's forward link
//
        gone->prev->next = gone->next;

//
// add it back in to the free list
//
        gone->prev = objfreelist;
        objfreelist = gone;

        objcount--;
}

/*
=============================================================================

                                                MUSIC STUFF

=============================================================================
*/


/*
=================
=
= StopMusic
=
=================
*/
int StopMusic(void)
{
        int lastoffs = SD_MusicOff();

        UNCACHEAUDIOCHUNK(STARTMUSIC + lastmusicchunk);

        return lastoffs;
}

//==========================================================================


/*
=================
=
= StartMusic
=
=================
*/

void StartMusic()
{
        SD_MusicOff();
        lastmusicchunk = (musicnames) songs[gamestate.mapon+gamestate.episode*10];

        CA_CacheAudioChunk(STARTMUSIC + lastmusicchunk);
        if(audiosegs[STARTMUSIC + lastmusicchunk])
                SD_StartMusic((MusicGroup *)audiosegs[STARTMUSIC + lastmusicchunk]);
}

void ContinueMusic(int offs)
{
        SD_MusicOff();
        lastmusicchunk = (musicnames) songs[gamestate.mapon+gamestate.episode*10];

        CA_CacheAudioChunk(STARTMUSIC + lastmusicchunk);
        if(audiosegs[STARTMUSIC + lastmusicchunk])
                SD_ContinueMusic((MusicGroup *) audiosegs[STARTMUSIC + lastmusicchunk],
                        offs);
}

/*
=============================================================================

                                        PALETTE SHIFTING STUFF

=============================================================================
*/

#define NUMREDSHIFTS    6
#define REDSTEPS                8

#define NUMWHITESHIFTS  3
#define WHITESTEPS              20
#define WHITETICS               6


byte    redshifts[NUMREDSHIFTS][768];
byte    whiteshifts[NUMREDSHIFTS][768];

int             damagecount,bonuscount;
boolean palshifted;

extern  byte    gamepal[768];

/*
=====================
=
= InitRedShifts
=
=====================
*/

void InitRedShifts (void)
{
        byte    *workptr, *baseptr;
        int             i,j,delta;


//
// fade through intermediate frames
//
        for (i=1;i<=NUMREDSHIFTS;i++)
        {
                workptr = (byte *)&redshifts[i-1][0];
                baseptr = gamepal;

                for (j=0;j<=255;j++)
                {
                        delta = 64-*baseptr;
                        *workptr++ = *baseptr++ + delta * i / REDSTEPS;
                        delta = -*baseptr;
                        *workptr++ = *baseptr++ + delta * i / REDSTEPS;
                        delta = -*baseptr;
                        *workptr++ = *baseptr++ + delta * i / REDSTEPS;
                }
        }

        for (i=1;i<=NUMWHITESHIFTS;i++)
        {
                workptr = (byte *)&whiteshifts[i-1][0];
                baseptr = gamepal;

                for (j=0;j<=255;j++)
                {
                        delta = 64-*baseptr;
                        *workptr++ = *baseptr++ + delta * i / WHITESTEPS;
                        delta = 62-*baseptr;
                        *workptr++ = *baseptr++ + delta * i / WHITESTEPS;
                        delta = 0-*baseptr;
                        *workptr++ = *baseptr++ + delta * i / WHITESTEPS;
                }
        }
}


/*
=====================
=
= ClearPaletteShifts
=
=====================
*/

void ClearPaletteShifts (void)
{
        bonuscount = damagecount = 0;
}


/*
=====================
=
= StartBonusFlash
=
=====================
*/

void StartBonusFlash (void)
{
        bonuscount = NUMWHITESHIFTS*WHITETICS;          // white shift palette
}


/*
=====================
=
= StartDamageFlash
=
=====================
*/

void StartDamageFlash (int damage)
{
        damagecount += damage;
}


/*
=====================
=
= UpdatePaletteShifts
=
=====================
*/

void UpdatePaletteShifts (void)
{
        int     red,white;

        if (bonuscount)
        {
                white = bonuscount/WHITETICS +1;
                if (white>NUMWHITESHIFTS)
                        white = NUMWHITESHIFTS;
                bonuscount -= tics;
                if (bonuscount < 0)
                        bonuscount = 0;
        }
        else
                white = 0;


        if (damagecount)
        {
                red = damagecount/10 +1;
                if (red>NUMREDSHIFTS)
                        red = NUMREDSHIFTS;

                damagecount -= tics;
                if (damagecount < 0)
                        damagecount = 0;
        }
        else
                red = 0;

        if (red)
        {
                VW_WaitVBL(1);
                VL_SetPalette (redshifts[red-1]);
                palshifted = true;
        }
        else if (white)
        {
                VW_WaitVBL(1);
                VL_SetPalette (whiteshifts[white-1]);
                palshifted = true;
        }
        else if (palshifted)
        {
                VW_WaitVBL(1);
                VL_SetPalette (gamepal);                // back to normal
                palshifted = false;
        }
}


/*
=====================
=
= FinishPaletteShifts
=
= Resets palette to normal if needed
=
=====================
*/

void FinishPaletteShifts (void)
{
        if (palshifted)
        {
                palshifted = 0;
                VW_WaitVBL(1);
                VL_SetPalette (gamepal);
        }
}


/*
=============================================================================

                                                CORE PLAYLOOP

=============================================================================
*/


/*
=====================
=
= DoActor
=
=====================
*/

void DoActor (objtype *ob)
{
        void (*think)(objtype *);

        if (!ob->active && !areabyplayer[ob->areanumber])
                return;

        if (!(ob->flags&(FL_NONMARK|FL_NEVERMARK)) )
                actorat[ob->tilex][ob->tiley] = NULL;

//
// non transitional object
//

        if (!ob->ticcount)
        {
                think = (void (*)(objtype *)) ob->state->think;
                if (think)
                {
                        think (ob);
                        if (!ob->state)
                        {
                                RemoveObj (ob);
                                return;
                        }
                }

                if (ob->flags&FL_NEVERMARK)
                        return;

                if ( (ob->flags&FL_NONMARK) && actorat[ob->tilex][ob->tiley])
                        return;

                actorat[ob->tilex][ob->tiley] = ob;
                return;
        }

//
// transitional object
//
        ob->ticcount-=(short)tics;
        while ( ob->ticcount <= 0)
        {
                think = (void (*)(objtype *)) ob->state->action;                        // end of state action
                if (think)
                {
                        think (ob);
                        if (!ob->state)
                        {
                                RemoveObj (ob);
                                return;
                        }
                }

                ob->state = ob->state->next;

                if (!ob->state)
                {
                        RemoveObj (ob);
                        return;
                }

                if (!ob->state->tictime)
                {
                        ob->ticcount = 0;
                        goto think;
                }

                ob->ticcount += ob->state->tictime;
        }

think:
        //
        // think
        //
        think = (void (*)(objtype *)) ob->state->think;
        if (think)
        {
                think (ob);
                if (!ob->state)
                {
                        RemoveObj (ob);
                        return;
                }
        }

        if (ob->flags&FL_NEVERMARK)
                return;

        if ( (ob->flags&FL_NONMARK) && actorat[ob->tilex][ob->tiley])
                return;

        actorat[ob->tilex][ob->tiley] = ob;
}

//==========================================================================


/*
===================
=
= PlayLoop
=
===================
*/
long funnyticount;


void PlayLoop (void)
{
        playstate = ex_stillplaying;
        TimeCount = lasttimecount = 0;
        frameon = 0;
        running = false;
        anglefrac = 0;
        facecount = 0;
        funnyticount = 0;
        memset (buttonstate,0,sizeof(buttonstate));
        ClearPaletteShifts ();

        if (MousePresent)
                Mouse(MDelta);  // Clear accumulated mouse movement

        if (demoplayback)
                IN_StartAck ();

        do
        {
                PollControls();

//
// actor thinking
//
                madenoise = false;

                MoveDoors ();
                MovePWalls ();

                for (obj = player;obj;obj = obj->next)
                        DoActor (obj);

                UpdatePaletteShifts ();

                ThreeDRefresh ();

                //
                // MAKE FUNNY FACE IF BJ DOESN'T MOVE FOR AWHILE
                //
                #ifdef SPEAR
                funnyticount += tics;
                if (funnyticount > 30l*70)
                {
                        funnyticount = 0;
                        LatchDrawPic (17,4,BJWAITING1PIC+(US_RndT()&1));
                        facecount = 0;
                }
                #endif

                gamestate.TimeCount+=tics;

                SD_Poll ();
                UpdateSoundLoc();       // JAB
                if (screenfaded)
                        VW_FadeIn ();

                CheckKeys();

//
// debug aids
//
                if (singlestep)
                {
                        VW_WaitVBL(singlestep);
                        lasttimecount = TimeCount;
                }
                if (extravbls)
                        VW_WaitVBL(extravbls);

                if (demoplayback)
                {
                        if (IN_CheckAck ())
                        {
                                IN_ClearKeysDown ();
                                playstate = ex_abort;
                        }
                }
        } while (!playstate && !startgame);

        if (playstate != ex_died)
                FinishPaletteShifts ();
}

