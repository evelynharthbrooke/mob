#pragma once

#include "../utility.h"
#include "../context.h"

namespace mob
{

class task;

void add_task(std::unique_ptr<task> t);

template <class Task, class... Args>
Task& add_task(Args&&... args)
{
	auto sp = std::make_unique<Task>(std::forward<Args>(args)...);
	auto* p = sp.get();
	add_task(std::move(sp));
	return *p;
}

void run_task(const std::string& name);
void run_tasks(const std::vector<std::string>& names);;
void run_all_tasks();
void list_tasks(bool err=false);


class tool;

class task
{
public:
	task(const task&) = delete;
	task& operator=(const task&) = delete;

	virtual ~task();
	static void interrupt_all();

	const std::string& name() const;
	const std::vector<std::string>& names() const;

	virtual fs::path get_source_path() const = 0;
	virtual const std::string& get_version() const = 0;
	virtual const bool get_prebuilt() const = 0;

	virtual bool is_super() const;

	virtual void run();
	virtual void interrupt();
	virtual void join();

	virtual void fetch();
	virtual void build_and_install();

protected:
	template <class... Names>
	task(std::string name, Names&&... names)
		: task(std::vector<std::string>{name, std::forward<Names>(names)...})
	{
	}

	task(std::vector<std::string> names);

	const context& cx() const;
	void add_name(std::string s);

	void check_interrupted();

	virtual void do_fetch() {}
	virtual void do_build_and_install() {}
	virtual void do_clean_for_rebuild() {}

	template <class Tool>
	auto run_tool(Tool&& t)
	{
		run_tool_impl(&t);
		return t.result();
	}

	void threaded_run(std::string name, std::function<void ()> f);

	void parallel(std::vector<std::pair<std::string, std::function<void ()>>> v)
	{
		std::vector<std::thread> ts;

		for (auto&& [name, f] : v)
		{
			cx().trace(context::generic, "running in parallel: " + name);

			ts.push_back(std::thread([this, name, f]
			{
				threaded_run(name, f);
			}));
		}

		for (auto&& t : ts)
			t.join();
	}

private:
	struct thread_context
	{
		std::thread::id tid;
		context cx;

		thread_context(std::thread::id tid, context cx)
			: tid(tid), cx(std::move(cx))
		{
		}
	};

	std::vector<std::string> names_;
	std::thread thread_;
	std::atomic<bool> interrupted_;

	std::vector<std::unique_ptr<thread_context>> contexts_;
	mutable std::mutex contexts_mutex_;

	std::vector<tool*> tools_;
	mutable std::mutex tools_mutex_;

	static std::mutex interrupt_mutex_;

	void clean_for_rebuild();
	void run_tool_impl(tool* t);
};


template <class Task>
class basic_task : public task
{
public:
	using task::task;

	fs::path get_source_path() const override
	{
		return Task::source_path();
	}

	const std::string& get_version() const override
	{
		return Task::version();
	}

	const bool get_prebuilt() const override
	{
		return Task::prebuilt();
	}
};


class parallel_tasks : public task
{
public:
	parallel_tasks(bool super);

	template <class Task, class... Args>
	parallel_tasks& add_task(Args&&... args)
	{
		children_.push_back(
			std::make_unique<Task>(std::forward<Args>(args)...));

		return *this;
	}

	fs::path get_source_path() const override
	{
		return {};
	}

	const std::string& get_version() const override
	{
		static std::string s;
		return s;
	}

	const bool get_prebuilt() const override
	{
		return false;
	}

	bool is_super() const override;

	void run() override;
	void interrupt() override;
	void join() override;

	void fetch() override;
	void build_and_install() override;

protected:
	void do_fetch() override;
	void do_build_and_install() override;
	void do_clean_for_rebuild() override;

private:
	bool super_;
	std::vector<std::unique_ptr<task>> children_;
	std::vector<std::thread> threads_;
};

}	// namespace
