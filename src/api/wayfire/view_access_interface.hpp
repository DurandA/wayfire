#ifndef VIEW_ACCESS_INTERFACE_HPP
#define VIEW_ACCESS_INTERFACE_HPP

#include "wayfire/condition/access_interface.hpp"
#include "wayfire/view.hpp"
#include <string>
#include <tuple>

namespace wf
{

class view_access_interface_t : public access_interface_t
{
public:
    virtual ~view_access_interface_t() override;

    virtual variant_t get(const std::string &identifier, bool &error) override;

    void set_view(wayfire_view view);
private:
    wayfire_view _view;
};

} // End namespace wf.


#endif // VIEW_ACCESS_INTERFACE_HPP
