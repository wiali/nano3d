#include <stdio.h>

#include "n3d_bin.h"
#include "n3d_frame.h"

namespace {

// per frame bin clear
void bin_clear(n3d_bin_t& bin, uint32_t argb, float depth)
{
    const uint32_t height = bin.state_.height_;
    const uint32_t width = bin.state_.width_;
    const uint32_t pitch = bin.state_.pitch_;

    uint32_t* c = bin.state_.target_[n3d_target_pixel].uint32_;
    float* z = bin.state_.target_[n3d_target_depth].float_;

    for (uint32_t y = 0; y < height; ++y) {

        for (uint32_t x = 0; x < width; ++x) {
            c ? c[x] = argb: 0;
            z ? z[x] = depth: 0;
        }

        z += pitch;
        c += pitch;
    }
}
};

// process all pending messages in a bins queue
void n3d_bin_process(n3d_bin_t* bin)
{
    n3d_assert(bin);

    // note: bin_man will acquire the lock when its asked to receive work
    //       to do.  this lock scope will release the lock after we have
    //       finished processing the bin.
    n3d_assert(bin->lock_.atom_ == 1);
    n3d_scope_spinlock_t guard(bin->lock_, false);
    n3d_rasterizer_t::state_t& state = bin->state_;

    // while there are messages left to process
    while (true) {

        // try to pop a command from the queue
        n3d_command_t cmd;
        while (!bin->pipe_.pop(cmd)) {
            return;
        }

        switch (cmd.command_) {
        case (n3d_command_t::cmd_triangle):
            // rasterize a single triangle
            if (bin->rasterizer_) {
                n3d_assert(bin->rasterizer_->raster_proc_);
                bin->rasterizer_->raster_proc_(
                    state,
                    cmd.triangle_,
                    bin->rasterizer_->user_);
            }
            break;

        case (n3d_command_t::cmd_present):
            // present working buffer to the screen buffer
            n3d_atomic_inc(bin->frame_);
            n3d_assert(bin->counter_);
            n3d_atomic_dec(*bin->counter_);
            break;

        case (n3d_command_t::cmd_clear):
            // clear the bin
            bin_clear(*bin, cmd.clear_.color_, cmd.clear_.depth_);
            break;

        case (n3d_command_t::cmd_texture):
            // bind a new texture
            bin->state_.texure_ = cmd.texture_;
            state.texure_ = cmd.texture_;
            break;

        case (n3d_command_t::cmd_rasterizer):
            // change the rasterizer
            bin->rasterizer_ = cmd.rasterizer_;
            break;

        default:
            n3d_assert(!"unknown command");
        }
    }
}
