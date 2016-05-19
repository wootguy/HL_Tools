#ifndef WXINCLUDE_H
#define WXINCLUDE_H

#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

enum wxSharedIdentifier
{
	wxID_SHARED_LOWEST = wxID_HIGHEST + 1,

	//Messages window
	wxID_SHARED_MESSAGES_CLEAR,
	wxID_SHARED_MESSAGES_COMMAND,

	//Game configs panel
	wxID_SHARED_GAMECONFIGS_CONFIG_CHANGED,
	wxID_SHARED_GAMECONFIGS_EDIT,

	//Edit game configs dialog
	wxID_SHARED_EDITGAMECONFIGS_ADD,
	wxID_SHARED_EDITGAMECONFIGS_EDIT,
	wxID_SHARED_EDITGAMECONFIGS_REMOVE,

	//Process dialog
	wxID_PROCESSDLG_INPUT,
	wxID_PROCESSDLG_SENDINPUT,
	wxID_PROCESSDLG_TERMINATE,
	wxID_PROCESSDLG_CLOSE,

	wxID_SHARED_HIGHEST
};

#endif //WXINCLUDE_H