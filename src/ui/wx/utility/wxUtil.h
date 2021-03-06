#ifndef UI_UTILITY_WXUTIL_H
#define UI_UTILITY_WXUTIL_H

#include <vector>

#include "ui/wx/wxInclude.h"

#include <wx/display.h>

#include "utility/Color.h"

namespace wx
{
/**
*	Function pointer type used to compare video modes.
*/
using VideoModeComparator = bool ( * )( const wxVideoMode& lhs, const wxVideoMode& rhs );

/**
*	Compares video modes using the resolutions only.
*	@return true if the modes are identical, false otherwise.
*/
bool CompareVideoModeResolutions( const wxVideoMode& lhs, const wxVideoMode& rhs );

/**
*	Given a display, fills the vector videoModes with all supported video modes, using comparator to compare video modes.
*	@param display Display whose supported video modes will be queried.
*	@param videoModes Destination vector that will contain all video modes.
*	@param comparator Comparator to use when filtering the list.
*	@return true on success, false otherwise.
*/
bool GetUniqueVideoModes( const wxDisplay& display, std::vector<wxVideoMode>& videoModes, const VideoModeComparator comparator );

/**
*	Given a display, fills the vector videoModes with all supported video modes, comparing video modes using the resolution only.
*	@param display: Display whose supported video modes will be queried.
*	@param videoModes: Destination vector that will contain all video modes.
*	@return true on success, false otherwise.
*/
bool GetUniqueVideoModes( const wxDisplay& display, std::vector<wxVideoMode>& videoModes );

/**
*	Converts a Color instance to wxColor.
*/
wxColor ColorTowx( const Color& color );

/**
*	Converts a wxColor instance to Color.
*/
Color wxToColor( const wxColor& color );

/**
*	Launches the default text editor and opens the given file. If it fails to launch, an error is displayed to the user.
*	@param szFilename File to open.
*	@return true on success, false otherwise.
*/
bool LaunchDefaultTextEditor( const wxString& szFilename );

/**
*	@brief Given an array of checkboxes and a maximum number of columns, returns a sizer containing the checkboxes, arranged in columns.
*
*	<pre>
*	Example:
*	CheckBox 1, CheckBox 2, CheckBox 3, 2 columns:
*	
*	CheckBox 1	|	CheckBox 3
*	CheckBox 2	|
*	</pre>
*
*	@param ppCheckBoxes Array of checkboxes.
*	@param uiNumCheckBoxes Number of checkboxes.
*	@param uiNumColumns Number of columns.
*	@param flag Sizer flags.
*	@param border Sizer border flags.
*	@return Sizer containing the given checkboxes.
*/
wxSizer* CreateCheckBoxSizer( wxCheckBox** ppCheckBoxes, const size_t uiNumCheckBoxes, const size_t uiNumColumns, 
							  int flag = 0,
							  int border = 0 );

/**
*	Formats a list of command line parameters as a single string.
*	@param params List of parameters.
*	@tparam Container type that can be iterated over. Must store a pair of strings.
*/
template<typename T>
wxString FormatCommandLine( const T& parameters )
{
	wxString szParameters;

	for( const auto& param : parameters )
	{
		szParameters += wxString::Format( " \"%s\"", param.first );

		if( !param.second.empty() )
		{
			szParameters += wxString::Format( " \"%s\"", param.second );
		}
	}

	//Remove the space at the beginning.
	if( !szParameters.IsEmpty() )
	{
		szParameters.erase( szParameters.begin() );
	}

	return szParameters;
}

/**
*	Formats a program name and list of command line parameters as a single string.
*	@param szProgram Name of the program to execute.
*	@param params List of parameters.
*	@tparam Container type that can be iterated over. Must store a pair of strings.
*/
template<typename T>
wxString FormatCommandLine( const wxString& szProgram, const T& parameters )
{
	return wxString::Format( "\"%s\" %s", szProgram, FormatCommandLine( parameters ) );
}
}

#endif //UI_UTILITY_WXUTIL_H