#include <wayfire/plugin.hpp>
#include <wayfire/plugins/common/workspace-wall.hpp>

class test_plugin_t : public wf::plugin_interface_t
{
    std::unique_ptr<wf::workspace_wall_t> wall;
  public:
    void init() override
    {
        wall = std::make_unique<wf::workspace_wall_t> (this->output);
        wall->set_background_color({1, 0, 0, 1});
        wall->set_gap_size(20);

        int fullw = (1280 + 20) * 3 + 20;
        int fullh = (720 + 20) * 3 + 20;

        int rw = (1280 + 20) * 3 + 20;
        int rh = (720 + 20) * 2 + 20;

        wall->set_viewport({(rw - fullw) / 2 - 20, (rh - fullh) / 2 - 20, fullw, fullh / 2});


        wall->start_output_renderer();
    }
};

DECLARE_WAYFIRE_PLUGIN(test_plugin_t);
