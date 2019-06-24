#ifndef UTIL_JOBSYSTEM_H_
#define UTIL_JOBSYSTEM_H_

#include <vector>
#include <map>
#include <thread>

#include <mutex>
#include <condition_variable>
#include <atomic>

class JobSystemWorker;

typedef struct alignas(64) Job
{
private:
	void(*jobFunction) (Job*);
	Job *parent; // Can be nullptr, meaning this job has no parent
	std::atomic<uint64_t> unfinishedJobs; // Includes this job and all children jobs

public:
	void *usrData;

	friend class JobSystem;
	friend class JobSystemWorker;

} Job;

class JobSystem
{
	public:

	JobSystem(unsigned int maxWorkerCount);
	virtual ~JobSystem();

	Job *allocateJob(void(*jobFunction) (Job*));
	Job *allocateJobAsChild(Job *parent, void(*jobFunction) (Job*));

	void runJob(Job *job);
	void runJobs(const std::vector<Job*> &jobs);
	void runJobs(Job **jobs, size_t jobCount);
	void waitForJob(Job *job, bool doWorkWhileWaiting = true);

	uint32_t getWorkerCount();

	static void setInstance(JobSystem *instancePtr);
	static JobSystem *get();

	void test();

	private:

	static JobSystem *instance;

	uint64_t availableJobs;
	std::mutex availableJobs_mutex;
	std::condition_variable availableJobs_cond;

	std::vector<JobSystemWorker*> workers;
	std::map<std::thread::id, size_t> workersThreadIDMap;

	friend class JobSystemWorker;
};

#endif /* UTIL_JOBSYSTEM_H_ */