/**
 * @file   workqueue_test.cpp
 * @author Nat Goodspeed
 * @date   2021-10-07
 * @brief  Test for workqueue.
 * 
 * $LicenseInfo:firstyear=2021&license=viewerlgpl$
 * Copyright (c) 2021, Linden Research, Inc.
 * $/LicenseInfo$
 */

// Precompiled header
#include "linden_common.h"
// associated header
#include "workqueue.h"
// STL headers
// std headers
#include <chrono>
#include <deque>
// external library headers
// other Linden headers
#include "../test/lltut.h"
#include "llcond.h"
#include "llstring.h"
#include "stringize.h"

using namespace LL;
using namespace std::literals::chrono_literals; // ms suffix
using namespace std::literals::string_literals; // s suffix

/*****************************************************************************
*   TUT
*****************************************************************************/
namespace tut
{
    struct workqueue_data
    {
        WorkQueue queue{"queue"};
    };
    typedef test_group<workqueue_data> workqueue_group;
    typedef workqueue_group::object object;
    workqueue_group workqueuegrp("workqueue");

    template<> template<>
    void object::test<1>()
    {
        set_test_name("name");
        ensure_equals("didn't capture name", queue.getKey(), "queue");
        ensure("not findable", WorkQueue::getInstance("queue") == queue.getWeak().lock());
        WorkQueue q2;
        ensure("has no name", LLStringUtil::startsWith(q2.getKey(), "WorkQueue"));
    }

    template<> template<>
    void object::test<2>()
    {
        set_test_name("post");
        bool wasRun{ false };
        // We only get away with binding a simple bool because we're running
        // the work on the same thread.
        queue.post([&wasRun](){ wasRun = true; });
        queue.close();
        ensure("ran too soon", ! wasRun);
        queue.runUntilClose();
        ensure("didn't run", wasRun);
    }

    template<> template<>
    void object::test<3>()
    {
        set_test_name("postEvery");
        // record of runs
        using Shared = std::deque<WorkQueue::TimePoint>;
        // This is an example of how to share data between the originator of
        // postEvery(work) and the work item itself, since usually a WorkQueue
        // is used to dispatch work to a different thread. Neither of them
        // should call any of LLCond's wait methods: you don't want to stall
        // either the worker thread or the originating thread (conventionally
        // main). Use LLCond or a subclass even if all you want to do is
        // signal the work item that it can quit; consider LLOneShotCond.
        LLCond<Shared> data;
        auto start = WorkQueue::TimePoint::clock::now();
        auto interval = 100ms;
        queue.postEvery(
            interval,
            [&data, count = 0]
            () mutable
            {
                // record the timestamp at which this instance is running
                data.update_one(
                    [](Shared& data)
                    {
                        data.push_back(WorkQueue::TimePoint::clock::now());
                    });
                // by the 3rd call, return false to stop
                return (++count < 3);
            });
        // no convenient way to close() our queue while we've got a
        // postEvery() running, so run until we think we should have exhausted
        // the iterations
        queue.runFor(10*interval);
        // Take a copy of the captured deque.
        Shared result = data.get();
        ensure_equals("called wrong number of times", result.size(), 3);
        // postEvery() assumes you want the first call to happen right away.
        // Inject a fake start time that's (interval) earlier than that, to
        // make our too early/too late tests uniform for all entries.
        result.push_front(start - interval);
        for (size_t i = 1; i < result.size(); ++i)
        {
            auto diff = (result[i] - result[i-1]);
            try
            {
                ensure(STRINGIZE("call " << i << " too soon"), diff >= interval);
                ensure(STRINGIZE("call " << i << " too late"), diff < interval*1.5);
            }
            catch (const tut::failure&)
            {
                auto interval_ms = interval / 1ms;
                auto diff_ms = diff / 1ms;
                std::cerr << "interval " << interval_ms
                          << "ms; diff " << diff_ms << "ms" << std::endl;
                throw;
            }
        }
    }

    template<> template<>
    void object::test<4>()
    {
        set_test_name("postTo");
        WorkQueue main("main");
        auto qptr = WorkQueue::getInstance("queue");
        int result = 0;
        main.postTo(
            qptr,
            [](){ return 17; },
            // Note that a postTo() *callback* can safely bind a reference to
            // a variable on the invoking thread, because the callback is run
            // on the invoking thread.
            [&result](int i){ result = i; });
        // this should post the callback to main
        qptr->runOne();
        // this should run the callback
        main.runOne();
        ensure_equals("failed to run int callback", result, 17);

        std::string alpha;
        // postTo() handles arbitrary return types
        main.postTo(
            qptr,
            [](){ return "abc"s; },
            [&alpha](const std::string& s){ alpha = s; });
        qptr->runPending();
        main.runPending();
        ensure_equals("failed to run string callback", alpha, "abc");
    }
} // namespace tut
