#pragma once

// 仅用 strcpy/enum/POD class，不需要 <afxwin.h>。
// stockworkspace.h 在 STL 之前 include 本文件，
// 若 include <afxwin.h> 会拖入 windows.h 早于 winsock2.h，触发 MFC C1189。
#include <string.h>


enum TABLE_SCROLL_VIEW_HITTEST
{
	TSV_HT_NOWHERE = 0,
	TSV_HT_BUTTON_LEFT = 1,
	TSV_HT_BUTTON_RIGHT = 2,
	TSV_HT_THUMB = 3,
	TSV_HT_CLIENT = 4, 
	TSV_HT_CLIENT_LEFT = 5,
	TSV_HT_CLIENT_RIGHT = 6,

	TSV_HT_VERT_BUTTON_UP = 7,
	TSV_HT_VERT_BUTTON_DOWN = 8,
	TSV_HT_VERT_THUMB = 9,
	TSV_HT_VERT_CLIENT_UP = 10,
	TSV_HT_VERT_CLIENT_DOWN = 11,
};


enum SW_CELL_EDIT_TYPE
{
	SW_EDIT_NONE  = 0,
	SW_EDIT_TEXT  = 1,
	SW_EDIT_INT   = 2,
	SW_EDIT_FLOAT = 3,
};


class TABLE_VIEW_COLUMN_INFO
{
public:
	char columnName[64];
	float width;
	int align;
	int can_sort;
	int sort;
	float actualLeft;
	float actualWidth;
	int can_edit;   // 0 / 1
	int edit_type;  // SW_CELL_EDIT_TYPE
public:
	TABLE_VIEW_COLUMN_INFO(char* columnName, float width, int align, int can_sort)
	{
		strcpy(this->columnName, columnName);
		this->width = width;
		this->align = align;
		this->can_sort = can_sort;
		this->sort = 0;
		this->actualLeft = 0;
		this->actualWidth = 0;
		this->can_edit = 0;
		this->edit_type = SW_EDIT_NONE;
	}
	TABLE_VIEW_COLUMN_INFO(char* columnName, float width, int align, int can_sort,
		int can_edit, int edit_type = SW_EDIT_TEXT)
	{
		strcpy(this->columnName, columnName);
		this->width = width;
		this->align = align;
		this->can_sort = can_sort;
		this->sort = 0;
		this->actualLeft = 0;
		this->actualWidth = 0;
		this->can_edit = can_edit;
		this->edit_type = edit_type;
	}
};
