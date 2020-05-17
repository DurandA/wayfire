#include <wayfire/plugin.hpp>
#include <wayfire/output.hpp>
#include <wayfire/core.hpp>
#include <wayfire/view.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/render-manager.hpp>
#include <wayfire/workspace-stream.hpp>
#include <wayfire/workspace-manager.hpp>
#include <wayfire/signal-definitions.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <wayfire/util/duration.hpp>

#include <cmath>
#include <utility>

#include <wayfire/plugins/common/workspace-wall.hpp>
#include <wayfire/plugins/common/geometry-animation.hpp>
#include "vswipe-processing.hpp"

class vswipe : public wf::plugin_interface_t
{
  private:
    enum swipe_direction_t
    {
        HORIZONTAL = 0,
        VERTICAL,
        UNKNOWN,
    };

    struct {
        bool swiping = false;
        bool animating = false;
        swipe_direction_t direction;

        wf::pointf_t initial_deltas;
        double delta_prev = 0.0;
        double delta_last = 0.0;

        int vx = 0;
        int vy = 0;
        int vw = 0;
        int vh = 0;
    } state;

    std::unique_ptr<wf::workspace_wall_t> wall;
    wf::option_wrapper_t<bool> enable_horizontal{"vswipe/enable_horizontal"};
    wf::option_wrapper_t<bool> enable_vertical{"vswipe/enable_vertical"};
    wf::option_wrapper_t<bool> smooth_transition{"vswipe/enable_smooth_transition"};

    wf::option_wrapper_t<wf::color_t> background_color{"vswipe/background"};
    wf::option_wrapper_t<int> animation_duration{"vswipe/duration"};
    wf::animation::simple_animation_t smooth_delta{animation_duration};

    wf::option_wrapper_t<int> fingers{"vswipe/fingers"};
    wf::option_wrapper_t<double> gap{"vswipe/gap"};
    wf::option_wrapper_t<double> threshold{"vswipe/threshold"};
    wf::option_wrapper_t<double> delta_threshold{"vswipe/delta_threshold"};
    wf::option_wrapper_t<double> speed_factor{"vswipe/speed_factor"};
    wf::option_wrapper_t<double> speed_cap{"vswipe/speed_cap"};

  public:
    void init() override
    {
        grab_interface->name = "vswipe";
        grab_interface->capabilities = wf::CAPABILITY_MANAGE_COMPOSITOR;
        grab_interface->callbacks.cancel = [=] () { finalize_and_exit(); };

        wf::get_core().connect_signal("pointer_swipe_begin", &on_swipe_begin);
        wf::get_core().connect_signal("pointer_swipe_update", &on_swipe_update);
        wf::get_core().connect_signal("pointer_swipe_end", &on_swipe_end);

        wall = std::make_unique<wf::workspace_wall_t> (output);
        wall->connect_signal("frame", &this->on_frame);
    }

    wf::signal_connection_t on_frame = {[=] (wf::signal_data_t*)
    {
        if (!smooth_delta.running() && !state.swiping)
        {
            finalize_and_exit();
            return;
        }

        output->render->schedule_redraw();

        wf::point_t current_workspace = {state.vx, state.vy};
        int dx = 0, dy = 0;

        if (state.direction == HORIZONTAL) {
            dx = 1;
        } else if (state.direction == VERTICAL) {
            dy = 1;
        }

        wf::point_t next_ws = {current_workspace.x + dx, current_workspace.y + dy};
        auto g1 = wall->get_workspace_rectangle(current_workspace);
        auto g2 = wall->get_workspace_rectangle(next_ws);
        wall->set_viewport(wf::interpolate(g1, g2, -smooth_delta));
    }};

    template<class wlr_event> using event = wf::input_event_signal<wlr_event>;
    wf::signal_callback_t on_swipe_begin = [=] (wf::signal_data_t *data)
    {
        if (!enable_horizontal && !enable_vertical)
            return;

        if (output->is_plugin_active(grab_interface->name))
            return;

        auto ev = static_cast<
            event<wlr_event_pointer_swipe_begin>*> (data)->event;
        if (static_cast<int>(ev->fingers) != fingers)
            return;

        // Plugins are per output, swipes are global, so we need to handle
        // the swipe only when the cursor is on *our* (plugin instance's) output
        if (!(output->get_relative_geometry() & output->get_cursor_position()))
            return;

        state.swiping = true;
        state.direction = UNKNOWN;
        state.initial_deltas = {0.0, 0.0};
        smooth_delta.set(0, 0);

        state.delta_last = 0;
        state.delta_prev = 0;

        // We switch the actual workspace before the finishing animation,
        // so the rendering of the animation cannot dynamically query current
        // workspace again, so it's stored here
        auto grid = output->workspace->get_workspace_grid_size();
        auto ws = output->workspace->get_current_workspace();
        state.vw = grid.width;
        state.vh = grid.height;
        state.vx = ws.x;
        state.vy = ws.y;
    };

    void start_swipe(swipe_direction_t direction)
    {
        assert(direction != UNKNOWN);
        state.direction = direction;

        if (!output->activate_plugin(grab_interface))
            return;

        grab_interface->grab();
        wf::get_core().focus_output(output);

        auto ws = output->workspace->get_current_workspace();
        wall->set_background_color(background_color);
        wall->set_gap_size(gap);
        wall->set_viewport(wall->get_workspace_rectangle(ws));
        wall->start_output_renderer();
    }

    wf::signal_callback_t on_swipe_update = [&] (wf::signal_data_t *data)
    {
        if (!state.swiping)
            return;

        auto ev = static_cast<
            event<wlr_event_pointer_swipe_update>*> (data)->event;

        if (state.direction == UNKNOWN)
        {
            auto grid = output->workspace->get_workspace_grid_size();

            // XXX: how to determine this??
            static constexpr double initial_direction_threshold = 0.05;
            state.initial_deltas.x +=
                std::abs(ev->dx) / speed_factor;
            state.initial_deltas.y +=
                std::abs(ev->dy) / speed_factor;

            bool horizontal =
                state.initial_deltas.x > initial_direction_threshold;
            bool vertical =
                state.initial_deltas.y > initial_direction_threshold;

            horizontal &= state.initial_deltas.x > state.initial_deltas.y;
            vertical &= state.initial_deltas.y > state.initial_deltas.x;

            if (horizontal && grid.width > 1 && enable_horizontal)
            {
                start_swipe(HORIZONTAL);
            }
            else if (vertical && grid.height > 1 && enable_vertical)
            {
                start_swipe(VERTICAL);
            }

            if (state.direction == UNKNOWN)
                return;
        }

        const double cap = speed_cap;
        const double fac = speed_factor;

        state.delta_prev = state.delta_last;
        double current_delta_processed;
        if (state.direction == HORIZONTAL)
        {
            current_delta_processed = vswipe_process_delta(ev->dx,
                smooth_delta, state.vx, state.vw, cap, fac);
            state.delta_last = ev->dx;
        } else
        {
            current_delta_processed = vswipe_process_delta(ev->dy,
                smooth_delta, state.vy, state.vh, cap, fac);
            state.delta_last = ev->dy;
        }

        double new_delta_end = smooth_delta.end + current_delta_processed;
        double new_delta_start = smooth_transition ?  smooth_delta : new_delta_end;
        smooth_delta.animate(new_delta_start, new_delta_end);
    };

    wf::signal_callback_t on_swipe_end = [=] (wf::signal_data_t *data)
    {
        if (!state.swiping || !output->is_plugin_active(grab_interface->name))
        {
            state.swiping = false;
            return;
        }

        state.swiping = false;
        const double move_threshold = wf::clamp((double)threshold, 0.0, 1.0);
        const double fast_threshold = wf::clamp((double)delta_threshold, 0.0, 1000.0);

        int target_delta = 0;
        wf::point_t target_workspace = {state.vx, state.vy};

        switch (state.direction)
        {
            case UNKNOWN:
                target_delta = 0;
                break;
            case HORIZONTAL:
                target_delta = vswipe_finish_target(smooth_delta.end,
                    state.vx, state.vw, state.delta_prev + state.delta_last,
                    move_threshold, fast_threshold);
                target_workspace.x -= target_delta;
                break;
            case VERTICAL:
                target_delta = vswipe_finish_target(smooth_delta.end,
                    state.vy, state.vh, state.delta_prev + state.delta_last,
                    move_threshold, fast_threshold);
                target_workspace.y -= target_delta;
                break;
        }

        smooth_delta.animate(target_delta);
        output->workspace->set_workspace(target_workspace);
        state.animating = true;
    };

    void finalize_and_exit()
    {
        state.swiping = false;
        grab_interface->ungrab();
        output->deactivate_plugin(grab_interface);

        wall->stop_output_renderer();
        wall->set_viewport({0, 0, 0, 0});
        state.animating = false;
    }

    void fini() override
    {
        if (state.swiping)
            finalize_and_exit();

        wf::get_core().disconnect_signal("pointer_swipe_begin", &on_swipe_begin);
        wf::get_core().disconnect_signal("pointer_swipe_update", &on_swipe_update);
        wf::get_core().disconnect_signal("pointer_swipe_end", &on_swipe_end);
    }
};

DECLARE_WAYFIRE_PLUGIN(vswipe);
