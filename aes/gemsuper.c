/*
*       Copyright 1999, Caldera Thin Clients, Inc.
*                 2002-2017 The EmuTOS development team
*
*       This software is licenced under the GNU Public License.
*       Please see LICENSE.TXT for further information.
*
*                  Historical Copyright
*       -------------------------------------------------------------
*       GEM Application Environment Services              Version 2.3
*       Serial No.  XXXX-0000-654321              All Rights Reserved
*       Copyright (C) 1987                      Digital Research Inc.
*       -------------------------------------------------------------
*/

/* #define ENABLE_KDEBUG */

#include "config.h"
#include "portab.h"
#include "kprint.h"
#include "struct.h"
#include "basepage.h"
#include "obdefs.h"
#include "intmath.h"
#include "gemlib.h"
#include "crysbind.h"
#include "gem_rsc.h"

#include "gemsuper.h"
#include "gempd.h"
#include "gemaplib.h"
#include "geminit.h"
#include "gemevlib.h"
#include "gemmnlib.h"
#include "gemoblib.h"
#include "gemobed.h"
#include "gemfmlib.h"
#include "gemfslib.h"
#include "gemgrlib.h"
#include "gemgraf.h"
#include "gemgsxif.h"
#include "gemsclib.h"
#include "gemwmlib.h"
#include "gemrslib.h"
#include "gemshlib.h"
#include "gemfmalt.h"
#include "gemdosif.h"
#include "gemasm.h"

#include "string.h"

#if CONF_SERIAL_CONSOLE
#define ENABLE_KDEBUG
#endif

extern WORD super(WORD cx, AESPB *pcrys_blk);   /* called only from gemdosif.S */

GLOBAL WORD     gl_mnclick;

static WORD     dspcnt;
static LONG     ad_rso;



#ifdef ENABLE_KDEBUG
static void aestrace(const char* message)
{
    char appname[AP_NAMELEN+1];
    const char *src = rlr->p_name;
    char *dest = appname;

    while (dest < &appname[AP_NAMELEN] && *src != ' ')
        *dest++ = *src++;
    *dest++ = '\0';

    kprintf("AES: %s: %s\n", appname, message);
}
#else
#define aestrace(a)
#endif

static UWORD crysbind(WORD opcode, AESGLOBAL *pglobal, WORD control[], WORD int_in[], WORD int_out[], LONG addr_in[])
{
    LONG    maddr, buparm;
    OBJECT  *tree;
    WORD    mouse, ret;
    WORD    unsupported = FALSE;

    maddr = 0;
    ret = TRUE;

    switch(opcode)
    {
    /* Application Manager */
    case APPL_INIT:
        aestrace("appl_init()");
        pglobal->ap_version = AES_VERSION;  /* version number     */
        pglobal->ap_count = 1;              /* # concurrent procs */
        pglobal->ap_id = rlr->p_pid;
        tree = rs_trees[DESKTOP];
        pglobal->ap_private = tree[ROOT].ob_spec;
        pglobal->ap_2resv[1] = gl_nplanes;
        pglobal->ap_3resv = (LONG)&D;

        /* reset dispatcher count to let the app run a while */
        dspcnt = 0;
        ret = ap_init();
        break;
    case APPL_READ:
    case APPL_WRITE:
        ap_rdwr(opcode == APPL_READ ? MU_MESAG : MU_SDMSG,
                fpdnm(NULL, AP_RWID), AP_LENGTH, AP_PBUFF);
        break;
    case APPL_FIND:
        ret = ap_find(AP_PNAME);
        break;
    case APPL_TPLAY:
        ap_tplay((FPD *)AP_TBUFFER, AP_TLENGTH, AP_TSCALE);
        break;
    case APPL_TRECORD:
        ret = ap_trecd((FPD *)AP_TBUFFER, AP_TLENGTH);
        break;
#if CONF_WITH_PCGEM
    case APPL_YIELD:
        dsptch();
        break;
#endif
    case APPL_EXIT:
        aestrace("appl_exit()");
        ap_exit();
        break;

    /* Event Manager */
    case EVNT_KEYBD:
        ret = ev_block(MU_KEYBD, 0x0L);
        break;
    case EVNT_BUTTON:
        ret = ev_button(B_CLICKS, B_MASK, B_STATE, &EV_MX);
        break;
    case EVNT_MOUSE:
        ret = ev_mouse((MOBLK *)&MO_FLAGS, &EV_MX);
        break;
    case EVNT_MESAG:
        aestrace("evnt_mesag()");
        rlr->p_flags |= AP_MESAG;
        ap_rdwr(MU_MESAG, rlr, 16, ME_PBUFF);
        break;
    case EVNT_TIMER:
        ev_timer(MAKE_ULONG(T_HICOUNT, T_LOCOUNT));
        break;
    case EVNT_MULTI:
        aestrace("evnt_multi()");
        if (MU_FLAGS & MU_MESAG)
            rlr->p_flags |= AP_MESAG;
        if (MU_FLAGS & MU_TIMER)
            maddr = MAKE_ULONG(MT_HICOUNT, MT_LOCOUNT);
        buparm = combine_cms(MB_CLICKS,MB_MASK,MB_STATE);
        ret = ev_multi(MU_FLAGS, (MOBLK *)&MMO1_FLAGS, (MOBLK *)&MMO2_FLAGS,
                        maddr, buparm, MME_PBUFF, &EV_MX);
        break;
    case EVNT_DCLICK:
        ret = ev_dclick(EV_DCRATE, EV_DCSETIT);
        break;

    /* Menu Manager */
    case MENU_BAR:
        if (gl_mnppd == rlr || gl_mnppd == NULL)
            mn_bar(MM_ITREE, SHOW_IT, rlr->p_pid);
        else
            menu_tree[rlr->p_pid] = (SHOW_IT) ? MM_ITREE : 0x0L;
        break;
    case MENU_ICHECK:
        do_chg(MM_ITREE, ITEM_NUM, CHECKED, CHECK_IT, FALSE, FALSE);
        break;
    case MENU_IENABLE:
        do_chg(MM_ITREE, (ITEM_NUM & 0x7fff), DISABLED,
                !ENABLE_IT, ((ITEM_NUM & 0x8000) != 0x0), FALSE);
        break;
    case MENU_TNORMAL:
        if (gl_mntree == menu_tree[rlr->p_pid])
            do_chg(MM_ITREE, TITLE_NUM, SELECTED, !NORMAL_IT, TRUE, TRUE);
        break;
    case MENU_TEXT:
        tree = (OBJECT *)MM_ITREE;
        strcpy((char *)tree[ITEM_NUM].ob_spec,(char *)MM_PTEXT);
        break;
    case MENU_REGISTER:
        ret = mn_register(MM_PID, MM_PSTR);
        break;
    case MENU_UNREGISTER:
#if CONF_WITH_PCGEM
        /* distinguish between menu_unregister() and menu_popup() */
        if (IN_LEN == 1)
            mn_unregister( MM_MID );
        else
#endif
            unsupported = TRUE;
        break;
    case MENU_CLICK:
        /* distinguish between menu_click() and menu_attach() */
        /*
         * although menu_click() is PC-GEM only, it's always
         * enabled because the desktop uses it.
         */
        if (IN_LEN == 2) {
            if (MN_SETIT)
                gl_mnclick = MN_CLICK;
            ret = gl_mnclick;
        } else
            unsupported = TRUE;
        break;

    /* Object Manager */
    case OBJC_ADD:
        ob_add(OB_TREE, OB_PARENT, OB_CHILD);
        break;
    case OBJC_DELETE:
        ret = ob_delete(OB_TREE, OB_DELOB);
        break;
    case OBJC_DRAW:
        gsx_sclip((GRECT *)&OB_XCLIP);
        ob_draw(OB_TREE, OB_DRAWOB, OB_DEPTH);
        break;
    case OBJC_FIND:
        ret = ob_find(OB_TREE, OB_STARTOB, OB_DEPTH, OB_MX, OB_MY);
        break;
    case OBJC_OFFSET:
        ob_offset(OB_TREE, OB_OBJ, &OB_XOFF, &OB_YOFF);
        break;
    case OBJC_ORDER:
        ob_order(OB_TREE, OB_OBJ, OB_NEWPOS);
        break;
    case OBJC_EDIT:
        gsx_sclip(&gl_rfull);
        OB_ODX = OB_IDX;
        ret = ob_edit(OB_TREE, OB_OBJ, OB_CHAR, &OB_ODX, OB_KIND);
        break;
    case OBJC_CHANGE:
        gsx_sclip((GRECT *)&OB_XCLIP);
        ob_change(OB_TREE, OB_DRAWOB, OB_NEWSTATE, OB_REDRAW);
        break;

    /* Form Manager */
    case FORM_DO:
        ret = fm_do(FM_FORM, FM_START);
        break;
    case FORM_DIAL:
        ret = fm_dial(FM_TYPE, (GRECT *)&FM_X);
        break;
    case FORM_ALERT:
        ret = fm_alert(FM_DEFBUT, FM_ASTRING);
        break;
    case FORM_ERROR:
        ret = fm_error(FM_ERRNUM);
        break;
    case FORM_CENTER:
        ob_center(FM_FORM, (GRECT *)&FM_XC);
        break;
    case FORM_KEYBD:
        gsx_sclip(&gl_rfull);
        FM_OCHAR = FM_ICHAR;
        FM_ONXTOB = FM_INXTOB;
        ret = fm_keybd(FM_FORM, FM_OBJ, &FM_OCHAR, &FM_ONXTOB);
        break;
    case FORM_BUTTON:
        gsx_sclip(&gl_rfull);
        ret = fm_button(FM_FORM, FM_OBJ, FM_CLKS, &FM_ONXTOB);
        break;

    /* Graphics Manager */
    case GRAF_RUBBOX:
        gr_rubbox(GR_I1, GR_I2, GR_I3, GR_I4, &GR_O1, &GR_O2);
        break;
    case GRAF_DRAGBOX:
        gr_dragbox(GR_I1, GR_I2, GR_I3, GR_I4, (GRECT *)&GR_I5, &GR_O1, &GR_O2);
        break;
    case GRAF_MBOX:
        gr_movebox(GR_I1, GR_I2, GR_I3, GR_I4, GR_I5, GR_I6);
        break;
    case GRAF_GROWBOX:
        gr_growbox((GRECT *)&GR_I1, (GRECT *)&GR_I5);
        break;
    case GRAF_SHRINKBOX:
        gr_shrinkbox((GRECT *)&GR_I1, (GRECT *)&GR_I5);
        break;
    case GRAF_WATCHBOX:
        ret = gr_watchbox(GR_TREE, GR_OBJ, GR_INSTATE, GR_OUTSTATE);
        break;
    case GRAF_SLIDEBOX:
        ret = gr_slidebox(GR_TREE, GR_PARENT, GR_OBJ, GR_ISVERT);
        break;
    case GRAF_HANDLE:
        GR_WCHAR = gl_wchar;
        GR_HCHAR = gl_hchar;
        GR_WBOX = gl_wbox;
        GR_HBOX = gl_hbox;
        ret = gl_handle;
        break;
    case GRAF_MOUSE:
        if (GR_MNUMBER > USER_DEF)
        {
            if (GR_MNUMBER == M_OFF)
                gsx_moff();
            if (GR_MNUMBER == M_ON)
                gsx_mon();
        }
        else
        {
            if (GR_MNUMBER != USER_DEF)
            {
                switch(GR_MNUMBER) {
                case TEXT_CRSR:
                    mouse = MICE01;
                    break;
                case HOURGLASS:
                    mouse = MICE02;
                    break;
                case POINT_HAND:
                    mouse = MICE03;
                    break;
                case FLAT_HAND:
                    mouse = MICE04;
                    break;
                case THIN_CROSS:
                    mouse = MICE05;
                    break;
                case THICK_CROSS:
                    mouse = MICE06;
                    break;
                case OUTLN_CROSS:
                    mouse = MICE07;
                    break;
                default:
                    mouse = MICE00;
                    break;
                }
                maddr = rs_bitblk[mouse].bi_pdata;
            }
            else
                maddr = GR_MADDR;
            gsx_mfset((MFORM *)maddr);
        }
        break;
    case GRAF_MKSTATE:
        gr_mkstate(&GR_MX, &GR_MY, &GR_MSTATE, &GR_KSTATE);
        break;

    /* Scrap Manager */
    case SCRP_READ:
        ret = sc_read((BYTE*)SC_PATH);
        break;
    case SCRP_WRITE:
        ret = sc_write((const BYTE*)SC_PATH);
        break;
#if CONF_WITH_PCGEM
    case SCRP_CLEAR:
        ret = sc_clear();
        break;
#endif

    /* File Selector Manager */
    case FSEL_INPUT:
        ret = fs_input((BYTE*)FS_IPATH, (BYTE*)FS_ISEL, &FS_BUTTON, NULL);
        break;
    case FSEL_EXINPUT:
        ret = fs_input((BYTE*)FS_IPATH, (BYTE*)FS_ISEL, &FS_BUTTON, (BYTE *)FS_ILABEL);
        break;

    /* Window Manager */
    case WIND_CREATE:
        ret = wm_create(WM_KIND, (GRECT *)&WM_WX);
        break;
    case WIND_OPEN:
        wm_open(WM_HANDLE, (GRECT *)&WM_WX);
        break;
    case WIND_CLOSE:
        wm_close(WM_HANDLE);
        break;
    case WIND_DELETE:
        wm_delete(WM_HANDLE);
        break;
    case WIND_GET:
        wm_get(WM_HANDLE, WM_WFIELD, &WM_OX);
        break;
    case WIND_SET:
        wm_set(WM_HANDLE, WM_WFIELD, &WM_IX);
        break;
    case WIND_FIND:
        ret = wm_find(WM_MX, WM_MY);
        break;
    case WIND_UPDATE:
        wm_update(WM_BEGUP);
        break;
    case WIND_CALC:
        wm_calc(WM_WCTYPE, WM_WCKIND, WM_WCIX, WM_WCIY, WM_WCIW, WM_WCIH,
                &WM_WCOX, &WM_WCOY, &WM_WCOW, &WM_WCOH);
        break;
    case WIND_NEW:
        wm_new();
        break;

    /* Resource Manager */
    case RSRC_LOAD:
        ret = rs_load(pglobal, RS_PFNAME);
        break;
    case RSRC_FREE:
        ret = rs_free(pglobal);
        break;
    case RSRC_GADDR:
        ret = rs_gaddr(pglobal, RS_TYPE, RS_INDEX, &ad_rso);
        break;
    case RSRC_SADDR:
        ret = rs_saddr(pglobal, RS_TYPE, RS_INDEX, RS_INADDR);
        break;
    case RSRC_OBFIX:
        rs_obfix(RS_TREE, RS_OBJ);
        break;

    /* Shell Manager */
    case SHEL_READ:
        sh_read((BYTE*)SH_PCMD, (BYTE*)SH_PTAIL);
        break;
    case SHEL_WRITE:
        ret = sh_write(SH_DOEX, SH_ISGR, SH_ISCR, (const BYTE*)SH_PCMD, (const BYTE*)SH_PTAIL);
        break;
    case SHEL_GET:
        sh_get((void*)SH_PBUFFER, SH_LEN);
        break;
    case SHEL_PUT:
        sh_put((const void *)SH_PDATA, SH_LEN);
        break;
    case SHEL_FIND:
        ret = sh_find((BYTE*)SH_PATH);
        break;
    case SHEL_ENVRN:
        sh_envrn((BYTE**)SH_PATH, (const BYTE*)SH_SRCH);
        break;
#if CONF_WITH_PCGEM
    case SHEL_RDEF:
        sh_rdef((BYTE*)SH_LPCMD, (BYTE*)SH_LPDIR);
        break;
    case SHEL_WDEF:
        sh_wdef((const BYTE*)SH_LPCMD, (const BYTE*)SH_LPDIR);
        break;
#endif
    default:
        unsupported = TRUE;
        break;
    }

    if (unsupported)
    {
        KDEBUG(("Bad AES function %d\n", opcode));
        if (opcode != 0)    /* Ignore zero since some PRGs are this call */
            fm_show(ALNOFUNC, &opcode, 1);
        ret = -1;
    }

    return ret;
}


/*
 *  Routine that copies input parameters into local buffers, calls the
 *  appropriate routine via a case statement, copies return parameters
 *  from local buffers, and returns to the routine.
 */
static void xif(AESPB *pcrys_blk)
{
    WORD    control[C_SIZE];
    WORD    int_in[I_SIZE];
    WORD    int_out[O_SIZE];
    LONG    addr_in[AI_SIZE];

    memcpy(control, pcrys_blk->control, C_SIZE*sizeof(WORD));
    if (IN_LEN)
        memcpy(int_in, pcrys_blk->intin, min(IN_LEN,I_SIZE)*sizeof(WORD));
    if (AIN_LEN)
        memcpy(addr_in, pcrys_blk->addrin, min(AIN_LEN,AI_SIZE)*sizeof(LONG));

    int_out[0] = crysbind(OP_CODE, (AESGLOBAL *)pcrys_blk->global, control, int_in, int_out,
                                addr_in);

    if (OUT_LEN)
        memcpy(pcrys_blk->intout, int_out, OUT_LEN*sizeof(WORD));
    if (OP_CODE == RSRC_GADDR)
        pcrys_blk->addrout[0] = ad_rso;
}


/*
 *  Supervisor entry point.  Stack frame must be exactly like
 *  this if supret is to work.
 */
WORD super(WORD cx, AESPB *pcrys_blk)
{
    if (cx == 200)
        xif(pcrys_blk);

    if ((++dspcnt % 8) == 0 || cx == 201)
        dsptch();

    return 0;
}


#if CONF_DEBUG_AES_STACK

#define MARKER_BYTE 0xaa

UBYTE* check_min; /* Minimum stack address to check */
UBYTE* check_max; /* Maximum stack address to check */
ULONG max_usage = 0; /* Maximum stack usage for an AES call, since the beginning */
UBYTE* min_pointer = NULL; /* Minimum detected stack pointer, since the beginning */

/* Called when entering AES trap #2 */
void trapaes_debug_enter(void)
{
    UBYTE* bottom = (UBYTE*)rlr->p_uda->u_super;
    UBYTE* current = (UBYTE*)rlr->p_uda->u_spsuper;
    UBYTE* top = bottom + sizeof rlr->p_uda->u_super;

    kprintf("AES enter rlr=0x%08lx bottom=0x%08lx current=0x%08lx top=0x%08lx free=%ld\n",
        (ULONG)rlr, (ULONG)bottom, (ULONG)current, (ULONG)top, current - bottom);

    if (!min_pointer)
        min_pointer = top;

    /* Fill unused stack stack space with the marker */
    check_min = bottom;
    check_max = current;
    memset(check_min, MARKER_BYTE, check_max - check_min);
}

/* Called when exiting AES trap #2 */
void trapaes_debug_exit(void)
{
    UBYTE* bottom = (UBYTE*)rlr->p_uda->u_super;
    UBYTE* current = (UBYTE*)rlr->p_uda->u_spsuper;
    UBYTE* top = bottom + sizeof rlr->p_uda->u_super;
    UBYTE* p;
    ULONG used;
    ULONG recommended;

    /* Detect the minimum used stack pointer during this AES call */
    for (p = check_min; p < check_max && *p == MARKER_BYTE; p++);
    used = check_max - p;
    if (used > max_usage)
        max_usage = used;

    if (p < min_pointer)
        min_pointer = p;

    recommended = (top - min_pointer) + 512; /* Heuristic */

    kprintf("AES exit  rlr=0x%08lx bottom=0x%08lx current=0x%08lx top=0x%08lx free=%ld, used=%ld, max_usage=%ld, recommended STACK_SIZE=%ld\n",
        (ULONG)rlr, (ULONG)bottom, (ULONG)current, (ULONG)top, current - bottom, used, max_usage, recommended/4);
}

#endif /* CONF_DEBUG_AES_STACK */
