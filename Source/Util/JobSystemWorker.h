#ifndef UTIL_JOBSYSTEMWORKER_H_
#define UTIL_JOBSYSTEMWORKER_H_

#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

constexpr uint64_t jobSystemMaxJobCount = 8192; // ALWAYS keep as a power of 2
constexpr uint64_t jobSystemJobCountMask = jobSystemMaxJobCount - 1u;

class JobSystem;
struct Job;

class JobSystemWorker
{
	public:

	std::thread workerThread;
	size_t workerIndex;

	std::atomic<bool> shouldShutdown;
	std::atomic<bool> active;

	JobSystemWorker(JobSystem *jobSystemParentPtr);
	virtual ~JobSystemWorker();

	Job *allocateJob();
	Job *findJob();

	void executeJob(Job *job);

	void threadMainFunction();

	void push(Job *job);
	Job *pop();
	Job *steal();

	private:

	JobSystem *jobSystemParent;

	Job *jobPool;
	Job **jobDeque;

	uint64_t allocatedJobs;

	std::atomic<int64_t> bottom;
	std::atomic<int64_t> top;

	void finishJob(Job *job);
};

#endif /* UTIL_JOBSYSTEMWORKER_H_ */