#include "Util/JobSystem.h"

#include <Util/JobSystemWorker.h>

#include <common.h>

JobSystem *JobSystem::instance = nullptr;

JobSystem::JobSystem(unsigned int maxWorkerCount)
{
	availableJobs = 0;

	// This workerCount includes the main thread
	uint32_t workerCount = std::max<uint32_t>(std::min<uint32_t>(std::thread::hardware_concurrency(), maxWorkerCount), 2);

	for (uint32_t i = 0; i < workerCount - 1; i++)
	{
		JobSystemWorker *worker = new JobSystemWorker(this);
		worker->workerThread = std::thread(std::bind(&JobSystemWorker::threadMainFunction, worker));
		worker->workerIndex = workers.size();

		workers.push_back(worker);
		workersThreadIDMap[worker->workerThread.get_id()] = workers.size() - 1;
	}
	
	// Note that the main thread "worker" should always be added last
	workers.push_back(new JobSystemWorker(this));
	workers.back()->workerIndex = workers.size();
	workersThreadIDMap[std::this_thread::get_id()] = workers.size() - 1;
	
	for (size_t i = 0; i < workers.size(); i++)
		workers[i]->active = true;
}

JobSystem::~JobSystem()
{
	for (size_t i = 0; i < workers.size(); i++)
	{
		JobSystemWorker *worker = workers[i];
		worker->shouldShutdown = true;
		worker->active = false;
	}

	availableJobs_cond.notify_all();

	for (size_t i = 0; i < workers.size(); i++)
	{
		JobSystemWorker *worker = workers[i];

		// In this case it's the main thread
		if (worker->workerIndex == workers.size() - 1)
			continue;

		if (worker->workerThread.joinable())
			worker->workerThread.join();

		delete workers[i];
	}
}

constexpr uint32_t testDataWorkerSize = 32;

void jobSystemTestFunc(Job *job)
{
	uint32_t workerIndex = *reinterpret_cast<uint32_t*>(&job->padding[0]) * testDataWorkerSize;
	uint32_t *data = reinterpret_cast<uint32_t*>(job->usrData);

	for (uint32_t i = workerIndex; i < workerIndex + testDataWorkerSize; i++)
	{
		float s = std::cos(float(i));
		float f = std::asin(float(i));
		float t = std::atan2(float(i) * float(i), float(i));

		if (s * f * t != 0.015324654f)
			data[i] = 1;
		else
			data[i] = 2;
	}
}

void JobSystem::test()
{
	if (sizeof(Job) != 64)
		Log::get()->warn("The size of a Job is {} bytes and not 64!", sizeof(Job));

	double accumeT = 0.0;
	uint32_t testDataSize = 16384;
	uint32_t testCount = 1024;

	uint32_t *testData = new uint32_t[testDataSize];

	for (uint32_t t = 0; t < testCount; t++)
	{
		memset(testData, 0, sizeof(testData));

		auto sT = std::chrono::high_resolution_clock::now();
		Job *root = allocateJob(nullptr);

		for (uint32_t i = 0; i < testDataSize / testDataWorkerSize; i++)
		{
			Job *child = allocateJobAsChild(root, &jobSystemTestFunc);
			child->usrData = testData;
			memcpy(child->padding, &i, sizeof(i));

			runJob(child);
		}

		runJob(root);
		waitForJob(root, true);

		auto eT = std::chrono::high_resolution_clock::now() - sT;
		accumeT += (std::chrono::duration_cast<std::chrono::nanoseconds>(eT).count() / 1000000.0) / double(testCount);

		for (uint32_t i = 0; i < testDataSize; i++)
			if (testData[i] != 1 && testData[i] != 2)
				Log::get()->error("Found invalid value in testData at index {}", i);
	}

	Log::get()->info("JobSystem test took {}ms", accumeT);
}

Job *JobSystem::allocateJob(void(*jobFunction) (Job*))
{
	auto workerIt = workersThreadIDMap.find(std::this_thread::get_id());

	if (workerIt == workersThreadIDMap.end())
	{
		Log::get()->error("A thread not associated with the job system tried to allocate a job!");
		throw std::runtime_error("A thread not associated with the job system tried to allocate a job");
		return nullptr;
	}

	Job *job = workers[workerIt->second]->allocateJob();
	job->jobFunction = jobFunction;
	job->parent = nullptr;
	job->unfinishedJobs = 1;

	return job;
}

Job *JobSystem::allocateJobAsChild(Job *parent, void(*jobFunction) (Job*))
{
	auto workerIt = workersThreadIDMap.find(std::this_thread::get_id());

	if (workerIt == workersThreadIDMap.end())
	{
		Log::get()->error("A thread not associated with the job system tried to allocate a job!");
		throw std::runtime_error("A thread not associated with the job system tried to allocate a job");
		return nullptr;
	}

	parent->unfinishedJobs++;

	Job *job = workers[workerIt->second]->allocateJob();
	job->jobFunction = jobFunction;
	job->parent = parent;
	job->unfinishedJobs = 1;

	return job;
}

void JobSystem::runJob(Job *job)
{
	auto workerIt = workersThreadIDMap.find(std::this_thread::get_id());

	if (workerIt == workersThreadIDMap.end())
	{
		Log::get()->error("A thread not associated with the job system tried to run a job!");
		throw std::runtime_error("A thread not associated with the job system tried to run a job");
		return;
	}
	
	workers[workerIt->second]->push(job);

	{
		std::lock_guard<std::mutex> lck(availableJobs_mutex);
		availableJobs++;
	}
	availableJobs_cond.notify_all();
}

void JobSystem::runJobs(const std::vector<Job*> &jobs)
{
	auto workerIt = workersThreadIDMap.find(std::this_thread::get_id());

	if (workerIt == workersThreadIDMap.end())
	{
		Log::get()->error("A thread not associated with the job system tried to run a job!");
		throw std::runtime_error("A thread not associated with the job system tried to run a job");
		return;
	}

	for (Job *job : jobs)
		workers[workerIt->second]->push(job);

	{
		std::lock_guard<std::mutex> lck(availableJobs_mutex);
		availableJobs += uint64_t(jobs.size());
	}
	availableJobs_cond.notify_all();
}

void JobSystem::runJobs(Job **jobs, size_t jobCount)
{
	auto workerIt = workersThreadIDMap.find(std::this_thread::get_id());

	if (workerIt == workersThreadIDMap.end())
	{
		Log::get()->error("A thread not associated with the job system tried to run a job!");
		throw std::runtime_error("A thread not associated with the job system tried to run a job");
		return;
	}

	for (size_t i = 0; i < jobCount; i++)
		workers[workerIt->second]->push(jobs[i]);

	{
		std::lock_guard<std::mutex> lck(availableJobs_mutex);
		availableJobs += uint64_t(jobCount);
	}
	availableJobs_cond.notify_all();
}

void JobSystem::waitForJob(Job *job, bool doWorkWhileWaiting)
{
	if (!doWorkWhileWaiting)
	{
		while (job->unfinishedJobs > 0)
			std::this_thread::yield();
	}
	else
	{
		auto workerIt = workersThreadIDMap.find(std::this_thread::get_id());

		if (workerIt == workersThreadIDMap.end())
		{
			Log::get()->warn("A thread not associated with the job system tried to wait on a job and run jobs while waiting!");

			while (job->unfinishedJobs > 0)
				std::this_thread::yield();

			return;
		}

		Job *jobToDoWhileWaiting = nullptr;

		while (job->unfinishedJobs > 0)
		{
			if ((jobToDoWhileWaiting = workers[workerIt->second]->findJob()) != nullptr)
			{
				workers[workerIt->second]->executeJob(jobToDoWhileWaiting);
			}
		}
	}
}

uint32_t JobSystem::getWorkerCount()
{
	return (uint32_t) workers.size();
}

void JobSystem::setInstance(JobSystem *instancePtr)
{
	instance = instancePtr;
}

JobSystem *JobSystem::get()
{
	return instance;
}