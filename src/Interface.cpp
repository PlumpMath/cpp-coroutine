#include "stdafx.h"
#include "Interface.h"

#include <thread>
#include <atomic>
#include <condition_variable>
#include "Manager.h"
#include "ContextManager.h"
#include "SchedulerManager.h"
#include "Context.h"
#include "Scheduler.h"

namespace
{
    coro::CManager& Mng()
    {
        return coro::Get::Instance();
    }
}

void coro::Init(std::shared_ptr<ILog> log)
{
    Mng().Init(log);
}

void coro::Stop(const std::chrono::milliseconds& max_duration)
{
    Mng().SchedulerManager()->Stop(max_duration);
}

coro::tResumeHandle coro::CurrentResumeId()
{
    return CContext::CurrentResumeId();
}

uint32_t coro::CurrentSchedulerId()
{
    return CSchedulerManager::CurrentId();
}

void coro::yield()
{
    CContext::YieldImpl();
}

void coro::InterruptionPoint(void)
{
    CScheduler::InterruptionPoint();
}

void coro::AddScheduler(const uint32_t& id, const std::string& name, const uint32_t thread_count, tTask init_task)
{
    Mng().SchedulerManager()->Create(id, name, thread_count, std::move(init_task));
}

void coro::Run(tTask task, const uint32_t& sheduler_id, const size_t stack_size)
{
    Mng().SchedulerManager()->Add(sheduler_id,
                                  [task, stack_size]
                                  {
                                      coro::Get::Instance().ContextManager()->Run(std::move(task), stack_size, []{});
                                  });
}

void coro::SyncRun(tTask task,
                   const uint32_t& sheduler_id,
                   const std::chrono::milliseconds& max_duration,
                   const size_t stack_size)
{
    struct sync_meta {std::condition_variable cv; std::atomic<bool> is_finish;};
    auto ptr_meta = std::make_shared<sync_meta>();
    ptr_meta->is_finish = false;
    auto wrap_task = [task, stack_size, ptr_meta]
        {
            auto finish_task = [ptr_meta]
            {
                ptr_meta->is_finish = true;
                ptr_meta->cv.notify_all();
            };
            coro::Get::Instance().ContextManager()->Run(std::move(task), stack_size, std::move(finish_task));
        };
    Mng().SchedulerManager()->Add(sheduler_id, std::move(wrap_task));

    std::mutex finish_task_mutex;
    std::unique_lock<std::mutex> lck(finish_task_mutex);
    ptr_meta->cv.wait_for(lck, max_duration, [ptr_meta] {return (ptr_meta->is_finish == true);});
}

void coro::Resume(const tResumeHandle& resume_handle, const uint32_t& sheduler_id)
{
    Mng().SchedulerManager()->Add(sheduler_id,
                                  [resume_handle]
                                  {
                                      coro::Get::Instance().ContextManager()->Resume(resume_handle);
                                  });
}
