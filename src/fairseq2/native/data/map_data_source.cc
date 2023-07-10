// Copyright (c) Meta Platforms, Inc. and affiliates.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include "fairseq2/native/data/map_data_source.h"

#include <exception>

#include <oneapi/tbb.h>

#include "fairseq2/native/data/data_pipeline.h"
#include "fairseq2/native/detail/exception.h"

namespace fairseq2::detail {

map_data_source::map_data_source(
    std::unique_ptr<data_source> &&inner,
    map_fn &&fn,
    std::size_t num_parallel_calls,
    bool warn_only) noexcept
  : inner_{std::move(inner)},
    map_fn_{std::move(fn)},
    num_parallel_calls_{num_parallel_calls},
    warn_only_{warn_only}
{
    buffer_.reserve(num_parallel_calls);

    buffer_iter_ = buffer_.begin();
}

std::optional<data>
map_data_source::next()
{
    if (num_parallel_calls_ <= 1) {
        std::optional<data> d{};

        while ((d = inner_->next())) {
            d = invoke_function(*std::move(d));
            if (d)
                break;
        }

        return d;
    }

    do {
        // Yield a buffered example.
        for (; buffer_iter_ < buffer_.end(); ++buffer_iter_) {
            if (*buffer_iter_)
                return std::move(*buffer_iter_++);
        }
    // If we have exhausted all buffered examples, try to refill the buffer.
    } while (fill_buffer());

    return std::nullopt;
}

void
map_data_source::reset()
{
    buffer_.clear();

    buffer_iter_ = buffer_.begin();

    inner_->reset();
}

void
map_data_source::record_position(tape &t) const
{
    t.record(buffer_);

    t.record(buffer_iter_ - buffer_.begin());

    inner_->record_position(t);
}

void
map_data_source::reload_position(tape &t)
{
    buffer_ = t.read<std::vector<std::optional<data>>>();

    buffer_iter_ = buffer_.begin() + t.read<std::ptrdiff_t>();

    inner_->reload_position(t);
}

bool
map_data_source::fill_buffer()
{
    buffer_.clear();

    for (std::size_t i = 0; i < num_parallel_calls_; i++) {
        std::optional<data> d = inner_->next();
        if (!d)
            break;

        buffer_.push_back(std::move(d));
    }

    if (buffer_.empty())
        return false;

    // Apply the processor to all buffered examples.
    auto apply_function = [this](const tbb::blocked_range<std::size_t> &range)
    {
        for (auto i = range.begin(); i < range.end(); ++i)
            buffer_[i] = invoke_function(*std::move(buffer_[i]));
    };

    tbb::blocked_range<std::size_t> range{0, buffer_.size()};

    // Avoid threading overhead if we have just one example.
    if (buffer_.size() == 1)
        apply_function(range);
    else
        tbb::parallel_for(range, apply_function);

    buffer_iter_ = buffer_.begin();

    return true;
}

std::optional<data>
map_data_source::invoke_function(data &&d)
{
    try {
        return map_fn_(std::move(d));
    } catch (const data_pipeline_error &) {
        if (!warn_only_)
            throw;
    } catch (const std::exception &) {
        if (!warn_only_)
            throw_with_nested<data_pipeline_error>(
                "The map operation has failed. See nested exception for details.");
    }

    // TODO: warn

    return std::nullopt;
}

}  // namespace fairseq2::detail
