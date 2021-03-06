/***************************************************************************
                          menu.c  -  description
                             -------------------
    begin                : Thu Sep 20 2001
    copyright            : (C) 2001 by Michael Speck
    email                : kulkanie@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "manager.h"
#ifdef SOUND
#include "audio.h"
#endif

List *menus = 0; /* list of menus used to delete them later */
extern Font *mfont, *mhfont, *mcfont;
extern int gap_height; /* gap size between items */
extern Menu *cur_menu;
extern int motion, motion_x, motion_y; /* mouse motion event */
#ifdef SOUND
extern Sound_Chunk *wav_menu_motion, *wav_menu_click;
#endif

/*
====================================================================
Create menu.
====================================================================
*/
Menu *menu_create( char *name, int layout, int x, int y, int w, int h, int border )
{
    Menu *menu = calloc( 1, sizeof( Menu ) );
    /* name */
    menu->name = strdup( name );
    /* layout */
    menu->layout = layout;
    menu->x = x + border; menu->y = y + border;
    menu->w = w - border * 2; menu->h = h - border * 2;
    /* items are cleared normally */
    menu->items = list_create( LIST_AUTO_DELETE, item_delete );
    /* add menu to list menus */
    list_add( menus, menu );
    /* create caption */
    menu_add( menu, item_create_separator( name ) );
    ((Item*)menu->items->head.next->item)->font = mcfont;
    menu_add( menu, item_create_separator( "" ) );
    /* return menu */
    return menu;
}
/*
====================================================================
Delete menu and all submenus starting with root menu
====================================================================
*/
void menu_delete( void *pmenu )
{
    Menu *menu = (Menu*)pmenu;
    if ( !menu ) return;
    list_delete( menu->items );
    if ( menu->name ) free( menu->name );
    free( menu );
}
/*
====================================================================
Add item to menu.
====================================================================
*/
void menu_add( Menu *menu, Item *item )
{
    list_add( menu->items, item );
    if ( item->type == ITEM_LINK && (Menu*)item->link != menu )
        ((Menu*)item->link)->parent = menu;
}
/*
====================================================================
Adjust position and size of all entries according to layout.
====================================================================
*/
void menu_adjust( Menu *menu )
{
    int height, x, y;
    Item *item;
    if ( menu->layout == MENU_LAYOUT_CENTERED ) {
        /* get full height of all items + gaps */
        height = menu->items->count * mhfont->height +
                 ( menu->items->count - 1 ) * gap_height;
        /* get positions of items */
        y = menu->y + ( menu->h - height ) / 2;
        x = menu->x;
        /* set position */
        list_reset( menu->items );
        while( ( item = list_next( menu->items ) ) ) {
            item->x = x; item->y = y;
            item->w = menu->w; item->h = mhfont->height;
            y += gap_height + mhfont->height;
            item_adjust( item ); /* adjust alignment */
        }
        return;
    }
    fprintf( stderr, "menu_adjust: layout %i not supported\n", menu->layout );
}
/*
====================================================================
Select menu: clear current item and set extern variable cur_menu
====================================================================
*/
void menu_select( Menu *menu )
{
    if ( cur_menu ) menu_unselect_cur_item( cur_menu );
    cur_menu = menu;
    menu->cur_item = 0;
    hint_set( 0 );
}
/*
====================================================================
Show/hide all items
====================================================================
*/
void menu_hide( Menu *menu )
{
    Item *item;
    list_reset( menu->items );
    while ( ( item = list_next( menu->items ) ) )
        item_hide( item );
}
void menu_show( Menu *menu )
{
    Item *item;
    list_reset( menu->items );
    while ( ( item = list_next( menu->items ) ) )
        item_show( item );
}
/*
====================================================================
Update alpha of items and modify items according to event (if any)
====================================================================
*/
int menu_update( Menu *menu, SDL_Event *event, int ms )
{
    int result = ACTION_NONE;
    Item *item, *old_item;
    int item_used;
    void (*callback)(void);
    /* update alpha of all items */
    list_reset( menu->items );
    while ( ( item = list_next( menu->items ) ) )
        item_update_alpha( item, ms );
    /* if value::grab is set do only except keyup events */
    if ( menu->cur_item && menu->cur_item->type == ITEM_KEY && menu->cur_item->value->grab ) {
        if ( event && event->type == SDL_KEYDOWN )
            value_set_key( menu->cur_item->value, event->key.keysym.sym );
        return result;
    }
    /* if other event occured check */
    if ( event )
        switch ( event->type ) {
            case SDL_KEYDOWN:
                switch ( event->key.keysym.sym ) {
                    case SDLK_ESCAPE:
                        /* if ESCAPE is pressed go up to last menu or leave */
                        if ( strequal( _("Back"), ((Item*)menu->items->tail.prev->item)->name ) )
                            menu_select( ((Item*)menu->items->tail.prev->item)->link );
                        else
                            if ( strequal( _("Quit"), ((Item*)menu->items->tail.prev->item)->name ) )
                                result = ACTION_QUIT;
                        break;
                    case SDLK_RETURN:
                        if ( !menu->cur_item ) break;
                        item_used = 1;
                        /* callback */
                        callback = menu->cur_item->callback;
                        /* action */
                        switch ( menu->cur_item->type ) {
                            case ITEM_ACTION: result = menu->cur_item->item_id; break;
                            case ITEM_LINK: menu_select( menu->cur_item->link ); break;
                            case ITEM_KEY: value_grab( menu->cur_item->value ); break;
                            case ITEM_SWITCH:
                            case ITEM_SWITCH_X:
                            case ITEM_RANGE:
                                value_inc( menu->cur_item->value );
                                break;
                            default: item_used = 0; break;
                        }
                        if ( item_used ) {
#ifdef SOUND
                            sound_play( wav_menu_click );
#endif
                            if ( callback ) (callback)();
                        }
                        break;
                    case SDLK_RIGHT:
                        if ( !menu->cur_item ) break;
                        item_used = 1;
                        switch ( menu->cur_item->type ) {
                            case ITEM_SWITCH:
                            case ITEM_SWITCH_X:
                            case ITEM_RANGE:
                                value_inc( menu->cur_item->value );
                                break;
                            default: item_used = 0; break;
                        }
#ifdef SOUND
                        if ( item_used )
                            sound_play( wav_menu_click );
#endif
                        break;
                    case SDLK_LEFT:
                        if ( !menu->cur_item ) break;
                        item_used = 1;
                        switch ( menu->cur_item->type ) {
                            case ITEM_SWITCH:
                            case ITEM_SWITCH_X:
                            case ITEM_RANGE:
                                value_dec( menu->cur_item->value );
                                break;
                            default: item_used = 0; break;
                        }
#ifdef SOUND
                        if ( item_used )
                            sound_play( wav_menu_click );
#endif
                        break;
                    case SDLK_UP:
                        menu_up( menu );
                        hint_set( menu->cur_item->hint );
#ifdef SOUND
                        sound_play( wav_menu_click );
#endif
                        break;
                    case SDLK_DOWN:
                        menu_down( menu );
                        hint_set( menu->cur_item->hint );
#ifdef SOUND
                        sound_play( wav_menu_click );
#endif
                        break;
                    default:
                        /* if type is edit: edit! */
                        if ( menu->cur_item && menu->cur_item->type == ITEM_EDIT ) {
                            value_edit( menu->cur_item->value, event->key.keysym.sym, event->key.keysym.unicode );
#ifdef SOUND
                            sound_play( wav_menu_click );
#endif
                            break;
                        }
                        break;
                }
                break;
            case SDL_MOUSEBUTTONUP:
                if ( !menu->cur_item ) break;
                item_used = 1;
                callback = menu->cur_item->callback;
                switch ( menu->cur_item->type ) {
                    case ITEM_ACTION: result = menu->cur_item->item_id; break;
                    case ITEM_LINK: menu_select( menu->cur_item->link ); break;
                    case ITEM_SWITCH:
                    case ITEM_SWITCH_X:
                    case ITEM_RANGE:
                        if ( event->button.button == LEFT_BUTTON )
                            value_inc( menu->cur_item->value );
                        else
                            value_dec( menu->cur_item->value );
                        break;
                    case ITEM_KEY: value_grab( menu->cur_item->value ); break;
                    default: item_used = 0; break;
                }
                if ( item_used ) {
#ifdef SOUND
                    sound_play( wav_menu_click );
#endif
                    if ( callback ) (callback)();
                }
                break;
        }
    /* mouse motion is handled by event_filter so check motion */
    if ( motion ) {
        /* select item mouse pointer is on */
        old_item = menu->cur_item;
        menu_unselect_cur_item( menu );
        list_reset( menu->items );
        while( ( item = list_next( menu->items ) ) )
            if ( item_focus( item, motion_x, motion_y ) ) {
                menu_select_item( menu, item );
                if ( old_item != menu->cur_item ) {
#ifdef SOUND
                    sound_play( wav_menu_motion );
#endif
                }
                break;
            }
        if ( menu->cur_item ) 
            hint_set( menu->cur_item->hint );
        else
            hint_set( 0 );
    }
    return result;
}
/*
====================================================================
Select/unselect item: update menu::cur_item und item::highlight
====================================================================
*/
void menu_unselect_cur_item( Menu *menu )
{
    if ( menu->cur_item ) menu->cur_item->highlighted = 0;
    menu->cur_item = 0;
}
void menu_select_item( Menu *menu, Item *item )
{
    menu->cur_item = item;
    menu->cur_item->highlighted = 1;
}
/*
====================================================================
Go one valid menu item up or down.
====================================================================
*/
void menu_up( Menu *menu )
{
    List_Entry *entry;
    Item *item;
    int valid = 0;
    /* no selection so far: select first item after caption */
    if ( !menu->cur_item ) {
        menu_select_item( menu, list_get( menu->items, 2 ) );
        return;
    }
    /* get entry -- MUST exist so don't check */
    entry = list_entry( menu->items, menu->cur_item );
    /* get next entry */
    do {
        entry = entry->prev;
        if ( entry == &menu->items->head )
            entry = menu->items->tail.prev;
        item = (Item*)entry->item;
        if ( item->type == ITEM_SEPARATOR ) continue;
        valid = 1;
    } while ( !valid );
    /* set as current */
    menu_unselect_cur_item( menu );
    menu_select_item( menu, entry->item );
}
void menu_down( Menu *menu )
{
    List_Entry *entry;
    Item *item;
    int valid = 0;
    /* no selection so far: select first item after caption */
    if ( !menu->cur_item ) {
        menu_select_item( menu, list_get( menu->items, 2 ) );
        return;
    }
    /* get entry -- MUST exist so don't check */
    entry = list_entry( menu->items, menu->cur_item );
    /* get next entry */
    do {
        entry = entry->next;
        if ( entry == &menu->items->tail )
            entry = menu->items->head.next;
        item = (Item*)entry->item;
        if ( item->type == ITEM_SEPARATOR ) continue;
        valid = 1;
    } while ( !valid );
    /* set as current */
    menu_unselect_cur_item( menu );
    menu_select_item( menu, entry->item );
}
