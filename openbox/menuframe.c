#include "menuframe.h"
#include "client.h"
#include "menu.h"
#include "screen.h"
#include "grab.h"
#include "openbox.h"
#include "render/theme.h"

#define SEPARATOR_HEIGHT 5

#define FRAME_EVENTMASK (ButtonPressMask |ButtonMotionMask | EnterWindowMask |\
			 LeaveWindowMask)
#define TITLE_EVENTMASK (ButtonPressMask | ButtonMotionMask)
#define ENTRY_EVENTMASK (EnterWindowMask | LeaveWindowMask | \
                         ButtonPressMask | ButtonReleaseMask)

GList *menu_frame_visible;

static void menu_frame_render(ObMenuFrame *self);
static void menu_frame_update(ObMenuFrame *self);

static Window createWindow(Window parent, unsigned long mask,
			   XSetWindowAttributes *attrib)
{
    return XCreateWindow(ob_display, parent, 0, 0, 1, 1, 0,
			 RrDepth(ob_rr_inst), InputOutput,
                         RrVisual(ob_rr_inst), mask, attrib);
                       
}

ObMenuFrame* menu_frame_new(ObMenu *menu, ObClient *client)
{
    ObMenuFrame *self;
    XSetWindowAttributes attr;

    self = g_new0(ObMenuFrame, 1);
    self->type = Window_Menu;
    self->menu = menu;
    self->selected = NULL;
    self->client = client;

    attr.event_mask = FRAME_EVENTMASK;
    self->window = createWindow(RootWindow(ob_display, ob_screen),
                                   CWEventMask, &attr);
    attr.event_mask = TITLE_EVENTMASK;
    self->title = createWindow(self->window, CWEventMask, &attr);
    self->items = createWindow(self->window, 0, NULL);

    XMapWindow(ob_display, self->items);

    self->a_title = RrAppearanceCopy(ob_rr_theme->a_menu_title);
    self->a_items = RrAppearanceCopy(ob_rr_theme->a_menu);

    return self;
}

void menu_frame_free(ObMenuFrame *self)
{
    if (self) {
        XDestroyWindow(ob_display, self->items);
        XDestroyWindow(ob_display, self->title);
        XDestroyWindow(ob_display, self->window);

        RrAppearanceFree(self->a_items);
        RrAppearanceFree(self->a_title);

        g_free(self);
    }
}

ObMenuEntryFrame* menu_entry_frame_new(ObMenuEntry *entry, ObMenuFrame *frame)
{
    ObMenuEntryFrame *self;
    XSetWindowAttributes attr;

    self = g_new0(ObMenuEntryFrame, 1);
    self->entry = entry;
    self->frame = frame;

    attr.event_mask = ENTRY_EVENTMASK;
    self->window = createWindow(self->frame->items, CWEventMask, &attr);
    self->icon = createWindow(self->window, 0, NULL);
    self->text = createWindow(self->window, 0, NULL);
    self->bullet = createWindow(self->window, 0, NULL);

    XMapWindow(ob_display, self->window);
    XMapWindow(ob_display, self->text);

    self->a_normal = RrAppearanceCopy(ob_rr_theme->a_menu_item);
    self->a_disabled = RrAppearanceCopy(ob_rr_theme->a_menu_disabled);
    self->a_selected = RrAppearanceCopy(ob_rr_theme->a_menu_hilite);

    self->a_icon = RrAppearanceCopy(ob_rr_theme->a_clear_tex);
    self->a_icon->texture[0].type = RR_TEXTURE_RGBA;
    self->a_bullet = RrAppearanceCopy(ob_rr_theme->a_menu_bullet);
    self->a_bullet->texture[0].type = RR_TEXTURE_MASK;

    self->a_text_normal =
        RrAppearanceCopy(ob_rr_theme->a_menu_text_item);
    self->a_text_disabled =
        RrAppearanceCopy(ob_rr_theme->a_menu_text_disabled);
    self->a_text_selected =
        RrAppearanceCopy(ob_rr_theme->a_menu_text_hilite);

    return self;
}

void menu_entry_frame_free(ObMenuEntryFrame *self)
{
    if (self) {
        XDestroyWindow(ob_display, self->icon);
        XDestroyWindow(ob_display, self->text);
        XDestroyWindow(ob_display, self->bullet);
        XDestroyWindow(ob_display, self->window);

        RrAppearanceFree(self->a_normal);
        RrAppearanceFree(self->a_disabled);
        RrAppearanceFree(self->a_selected);

        RrAppearanceFree(self->a_icon);
        RrAppearanceFree(self->a_text_normal);
        RrAppearanceFree(self->a_text_disabled);
        RrAppearanceFree(self->a_text_selected);
        RrAppearanceFree(self->a_bullet);

        g_free(self);
    }
}

void menu_frame_move(ObMenuFrame *self, gint x, gint y)
{
    /* XXX screen constraints */
    RECT_SET_POINT(self->area, x, y);
    XMoveWindow(ob_display, self->window, self->area.x, self->area.y);
}

void menu_frame_move_on_screen(ObMenuFrame *self)
{
    Rect *a;
    guint i;
    gint dx = 0, dy = 0;

    for (i = 0; i < screen_num_monitors; ++i) {
        a = screen_physical_area_monitor(i);
        if (RECT_INTERSECTS_RECT(*a, self->area))
            break;
    }
    if (a) a = screen_physical_area_monitor(0);

    dx = MIN(0, (a->x + a->width) - (self->area.x + self->area.width));
    dy = MIN(0, (a->y + a->height) - (self->area.y + self->area.height));
    if (!dx) dx = MAX(0, a->x - self->area.x);
    if (!dy) dy = MAX(0, a->y - self->area.y);

    if (dx || dy) {
        ObMenuFrame *f;

        for (f = self; f; f = f->parent)
            menu_frame_move(f, f->area.x + dx, f->area.y + dy);
        XWarpPointer(ob_display, None, None, 0, 0, 0, 0, dx, dy);
    }
}

static void menu_entry_frame_render(ObMenuEntryFrame *self)
{
    RrAppearance *item_a, *text_a;
    gint th; /* temp */

    item_a = (!self->entry->enabled ?
              self->a_disabled :
              (self == self->frame->selected ?
               self->a_selected :
               self->a_normal));
    switch (self->entry->type) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
    case OB_MENU_ENTRY_TYPE_SUBMENU:
        th = self->frame->item_h;
        break;
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        th = SEPARATOR_HEIGHT;
        break;
    }
    RECT_SET_SIZE(self->area, self->frame->inner_w, th);
    XResizeWindow(ob_display, self->window,
                  self->area.width, self->area.height);
    item_a->surface.parent = self->frame->a_items;
    item_a->surface.parentx = self->area.x;
    item_a->surface.parenty = self->area.y;
    RrPaint(item_a, self->window, self->area.width, self->area.height);

    text_a = (!self->entry->enabled ?
              self->a_text_disabled :
              (self == self->frame->selected ?
               self->a_text_selected :
               self->a_text_normal));
    switch (self->entry->type) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
        text_a->texture[0].data.text.string = self->entry->data.normal.label;
        break;
    case OB_MENU_ENTRY_TYPE_SUBMENU:
        text_a->texture[0].data.text.string =
            self->entry->data.submenu.submenu->title;
        break;
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        break;
    }

    switch (self->entry->type) {
    case OB_MENU_ENTRY_TYPE_NORMAL:
    case OB_MENU_ENTRY_TYPE_SUBMENU:
        XMoveResizeWindow(ob_display, self->text,
                          self->frame->text_x, 0,
                          self->frame->text_w, self->frame->item_h);
        text_a->surface.parent = item_a;
        text_a->surface.parentx = self->frame->text_x;
        text_a->surface.parenty = 0;
        RrPaint(text_a, self->text, self->frame->text_w, self->frame->item_h);
        XMapWindow(ob_display, self->text);
        break;
    case OB_MENU_ENTRY_TYPE_SEPARATOR:
        XUnmapWindow(ob_display, self->text);
        break;
    }

    /* XXX draw icons */
    if (0) {
        XMoveResizeWindow(ob_display, self->icon, 0, 0,
                          self->frame->item_h, self->frame->item_h);
        /* XXX set the RGBA surface stuff */
        self->a_icon->surface.parent = item_a;
        self->a_icon->surface.parentx = 0;
        self->a_icon->surface.parenty = 0;
        RrPaint(self->a_icon, self->icon,
                self->frame->item_h, self->frame->item_h);
        XMapWindow(ob_display, self->icon);
    } else
        XUnmapWindow(ob_display, self->icon);

    if (self->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU) {
        XMoveResizeWindow(ob_display, self->bullet,
                          self->frame->text_x + self->frame->text_w, 0,
                          self->frame->item_h, self->frame->item_h);
        self->a_bullet->surface.parent = item_a;
        self->a_bullet->surface.parentx =
            self->frame->text_x + self->frame->text_w;
        self->a_bullet->surface.parenty = 0;
        RrPaint(self->a_bullet, self->bullet,
                self->frame->item_h, self->frame->item_h);
        XMapWindow(ob_display, self->bullet);
    } else
        XUnmapWindow(ob_display, self->bullet);
}

static void menu_frame_render(ObMenuFrame *self)
{
    gint w = 0, h = 0;
    gint allitems_h = 0;
    gint tw, th; /* temps */
    GList *it;
    gboolean has_icon = FALSE, has_bullet = FALSE;

    XSetWindowBorderWidth(ob_display, self->window, ob_rr_theme->bwidth);
    XSetWindowBorder(ob_display, self->window,
                     RrColorPixel(ob_rr_theme->b_color));

    if (!self->parent && self->menu->title) {
        XMoveWindow(ob_display, self->title, 
                    -ob_rr_theme->bwidth, h - ob_rr_theme->bwidth);

        self->a_title->texture[0].data.text.string = self->menu->title;
        RrMinsize(self->a_title, &tw, &th);
        w = MAX(w, tw);
        h += (self->title_h = th + ob_rr_theme->bwidth);

        XSetWindowBorderWidth(ob_display, self->title, ob_rr_theme->bwidth);
        XSetWindowBorder(ob_display, self->title,
                         RrColorPixel(ob_rr_theme->b_color));
    }

    XMoveWindow(ob_display, self->items, 0, h);

    if (self->entries) {
        ObMenuEntryFrame *e = self->entries->data;
        e->a_text_normal->texture[0].data.text.string = "";
        RrMinsize(e->a_text_normal, &tw, &th);
        self->item_h = th;
    } else
        self->item_h = 0;

    for (it = self->entries; it; it = g_list_next(it)) {
        RrAppearance *text_a;
        ObMenuEntryFrame *e = it->data;

        RECT_SET_POINT(e->area, 0, allitems_h);
        XMoveWindow(ob_display, e->window, 0, e->area.y);

        text_a = (!e->entry->enabled ?
                  e->a_text_disabled :
                  (e == self->selected ?
                   e->a_text_selected :
                   e->a_text_normal));
        switch (e->entry->type) {
        case OB_MENU_ENTRY_TYPE_NORMAL:
            text_a->texture[0].data.text.string = e->entry->data.normal.label;
            RrMinsize(text_a, &tw, &th);

            /* XXX has_icon = TRUE; */
            break;
        case OB_MENU_ENTRY_TYPE_SUBMENU:
            text_a->texture[0].data.text.string =
                e->entry->data.submenu.submenu->title;
            RrMinsize(text_a, &tw, &th);

            has_bullet = TRUE;
            break;
        case OB_MENU_ENTRY_TYPE_SEPARATOR:
            tw = 0;
            th = SEPARATOR_HEIGHT;
            break;
        }
        w = MAX(w, tw);
        h += th;
        allitems_h += th;
    }

    self->text_x = 0;
    self->text_w = w;

    if (self->entries) {
        if (has_bullet)
            w += self->item_h;
        if (has_icon) {
            w += self->item_h;
            self->text_x += self->item_h;
        }
    }

    if (!w) w = 10;
    if (!allitems_h) allitems_h = 3;
    if (!h) h = 3;

    XResizeWindow(ob_display, self->window, w, h);
    XResizeWindow(ob_display, self->items, w, allitems_h);

    self->inner_w = w;

    if (!self->parent && self->title) {
        XResizeWindow(ob_display, self->title,
                      w, self->title_h - ob_rr_theme->bwidth);
        RrPaint(self->a_title, self->title,
                w, self->title_h - ob_rr_theme->bwidth);
        XMapWindow(ob_display, self->title);
    } else
        XUnmapWindow(ob_display, self->title);

    RrPaint(self->a_items, self->items, w, allitems_h);

    for (it = self->entries; it; it = g_list_next(it))
        menu_entry_frame_render(it->data);

    w += ob_rr_theme->bwidth * 2;
    h += ob_rr_theme->bwidth * 2;

    RECT_SET_SIZE(self->area, w, h);
}

static void menu_frame_update(ObMenuFrame *self)
{
    GList *mit, *fit;

    self->selected = NULL;

    for (mit = self->menu->entries, fit = self->entries; mit && fit;
         mit = g_list_next(mit), fit = g_list_next(fit))
    {
        ObMenuEntryFrame *f = fit->data;
        f->entry = mit->data;
    }

    while (mit) {
        ObMenuEntryFrame *e = menu_entry_frame_new(mit->data, self);
        self->entries = g_list_append(self->entries, e);
        mit = g_list_next(mit);
    }
    
    while (fit) {
        GList *n = g_list_next(fit);
        menu_entry_frame_free(fit->data);
        self->entries = g_list_delete_link(self->entries, fit);
        fit = n;
    }

    menu_frame_render(self);
}

void menu_frame_show(ObMenuFrame *self, ObMenuFrame *parent)
{
    if (parent) {
        if (parent->child)
            menu_frame_hide(parent->child);
        parent->child = self;
    }
    self->parent = parent;

    if (menu_frame_visible == NULL) {
        /* no menus shown yet */
        grab_pointer(TRUE, None);
        grab_keyboard(TRUE);
    }

    if (!g_list_find(menu_frame_visible, self)) {
        menu_frame_visible = g_list_prepend(menu_frame_visible, self);

        menu_frame_update(self);
    }

    menu_frame_move_on_screen(self);

    XMapWindow(ob_display, self->window);
}

void menu_frame_hide(ObMenuFrame *self)
{
    menu_frame_visible = g_list_remove(menu_frame_visible, self);

    if (self->child)
        menu_frame_hide(self->child);

    if (self->parent)
        self->parent->child = NULL;
    self->parent = NULL;

    if (menu_frame_visible == NULL) {
        /* last menu shown */
        grab_pointer(FALSE, None);
        grab_keyboard(FALSE);
    }

    XUnmapWindow(ob_display, self->window);

    menu_frame_free(self);
}

void menu_frame_hide_all()
{
    while (menu_frame_visible)
        menu_frame_hide(menu_frame_visible->data);
}

ObMenuFrame* menu_frame_under(gint x, gint y)
{
    ObMenuFrame *ret = NULL;
    GList *it;

    for (it = menu_frame_visible; it; it = g_list_next(it)) {
        ObMenuFrame *f = it->data;

        if (RECT_CONTAINS(f->area, x, y)) {
            ret = f;
            break;
        }
    }
    return ret;
}

ObMenuEntryFrame* menu_entry_frame_under(gint x, gint y)
{
    ObMenuFrame *frame;
    ObMenuEntryFrame *ret = NULL;
    GList *it;

    if ((frame = menu_frame_under(x, y))) {
        x -= ob_rr_theme->bwidth + frame->area.x;
        y -= frame->title_h + ob_rr_theme->bwidth + frame->area.y;

        for (it = frame->entries; it; it = g_list_next(it)) {
            ObMenuEntryFrame *e = it->data;

            if (RECT_CONTAINS(e->area, x, y)) {
                ret = e;            
                break;
            }
        }
    }
    return ret;
}

void menu_frame_select(ObMenuFrame *self, ObMenuEntryFrame *entry)
{
    ObMenuEntryFrame *old = self->selected;

    if (old == entry) return;

    if (entry && entry->entry->type != OB_MENU_ENTRY_TYPE_SEPARATOR)
        self->selected = entry;
    else
        self->selected = NULL;

    if (old) {
        menu_entry_frame_render(old);
        if (old->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)
            menu_frame_hide(self->child);
    }
    if (self->selected) {
        menu_entry_frame_render(self->selected);

        if (self->selected->entry->type == OB_MENU_ENTRY_TYPE_SUBMENU)
            menu_entry_frame_show_submenu(self->selected);
    }
}

void menu_entry_frame_show_submenu(ObMenuEntryFrame *self)
{
    ObMenuFrame *f;

    f = menu_frame_new(self->entry->data.submenu.submenu,
                       self->frame->client);
    menu_frame_move(f,
                    self->frame->area.x + self->frame->area.width
                    - ob_rr_theme->menu_overlap,
                    self->frame->area.y + self->frame->title_h +
                    self->area.y);
    menu_frame_show(f, self->frame);
}

void menu_entry_frame_execute(ObMenuEntryFrame *self)
{
    if (self->entry->type == OB_MENU_ENTRY_TYPE_NORMAL) {
        GSList *it;

        for (it = self->entry->data.normal.actions; it;
             it = g_slist_next(it))
        {
            ObAction *act = it->data;
            act->data.any.c = self->frame->client;
            act->func(&act->data);
        }
        menu_frame_hide_all();
    }
}