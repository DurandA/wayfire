#include "wayfire/condition/access_interface.hpp"
#include "wayfire/view.hpp"
#include "wayfire/view_access_interface.hpp"
#include <algorithm>
#include <iostream>
#include <string>

namespace wf
{

view_access_interface_t::~view_access_interface_t()
{
}

variant_t view_access_interface_t::get(const std::string &identifier, bool &error)
{
    variant_t out = std::string(""); // Default to empty string as output.
    error = false; // Assume things will go well.

    if (identifier == "app_id")
    {
        out = _view->get_app_id();
    }
    else if (identifier == "title")
    {
        out = _view->get_title();
    }
    else if (identifier == "role")
    {
        switch (_view->role)
        {
        case VIEW_ROLE_TOPLEVEL:
            out = std::string("TOPLEVEL");
            break;
        case VIEW_ROLE_UNMANAGED:
            out = std::string("UNMANAGED");
            break;
        case VIEW_ROLE_DESKTOP_ENVIRONMENT:
            out = std::string("DESKTOP_ENVIRONMENT");
            break;
        default:
            std::cerr << "View access interface: View has unsupported value for role: " << static_cast<int>(_view->role) << std::endl;
            error = true;
            break;
        }
    }
    else if (identifier == "fullscreen")
    {
        out = _view->fullscreen;
    }
    else if (identifier == "activated")
    {
        out = _view->activated;
    }
    else if (identifier == "minimized")
    {
        out = _view->minimized;
    }
    else
    {
        std::cerr << "View access interface: Get operation triggered to unsupported view property " << identifier << std::endl;
    }

    return out;
}

void view_access_interface_t::set_view(wayfire_view view)
{
    _view = view;
}

} // End namespace wf.
