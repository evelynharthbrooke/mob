#include "pch.h"
#include "tasks.h"

namespace mob
{

openssl::openssl()
	: basic_task("openssl")
{
}

const std::string& openssl::version()
{
	return versions::by_name("openssl");
}

bool openssl::prebuilt()
{
	return false;
}

fs::path openssl::source_path()
{
	return paths::build() / ("openssl-" + version());
}

fs::path openssl::build_path()
{
	return source_path() / "build";
}

void openssl::do_clean_for_rebuild()
{
	cx().debug(context::rebuild,
		"openssl puts object files everywhere, so the whole tree will be "
		"deleted for a rebuild");

	op::delete_directory(cx(), source_path(), op::optional);
}

void openssl::do_fetch()
{
	const auto file = run_tool(downloader(source_url()));

	run_tool(extractor()
		.file(file)
		.output(source_path()));
}

void openssl::do_build_and_install()
{
	if (fs::exists(source_path() / "makefile"))
		cx().trace(context::bypass, "openssl already configured");
	else
		configure();

	install_engines();
	copy_files();
}

void openssl::configure()
{
	run_tool(process_runner(process()
		.binary(tools::perl::binary())
		.arg("Configure")
		.arg("--openssldir=", build_path())
		.arg("--prefix=", build_path())
		.arg("-FS")
		.arg("-MP1")
		.arg("VC-WIN64A")
		.cwd(source_path())
		.env(env::vs(arch::x64))));
}

void openssl::install_engines()
{
	const int max_tries = 3;

	for (int tries=0; tries<max_tries; ++tries)
	{
		int exit_code = run_tool(jom()
			.path(source_path())
			.target("install_engines")
			.flag(jom::allow_failure));

		if (exit_code == 0)
			return;

		cx().debug(context::generic,
			"jom /J regularly fails with openssh because of race conditions; "
			"trying again");
	}

	cx().debug(context::generic,
		"jom /J has failed more than " + std::to_string(max_tries) + " "
		"times, restarting one last time without /J; that one should work");

	run_tool(jom()
		.path(source_path())
		.target("install_engines")
		.flag(jom::single_job));
}

void openssl::copy_files()
{
	op::copy_file_to_dir_if_better(cx(),
		source_path() / "ms" / "applink.c",
		include_path());

	copy_dlls_to(paths::install_bin());
	copy_dlls_to(paths::install_dlls());
	copy_pdbs_to(paths::install_pdbs());
}

void openssl::copy_dlls_to(const fs::path& dir)
{
	for (auto&& name : output_names())
	{
		op::copy_file_to_dir_if_better(cx(),
			build_path() / "bin" / (name + ".dll"), dir);
	}
}

void openssl::copy_pdbs_to(const fs::path& dir)
{
	for (auto&& name : output_names())
	{
		op::copy_file_to_dir_if_better(cx(),
			build_path() / "bin" / (name + ".pdb"), dir);
	}
}

fs::path openssl::include_path()
{
	return openssl::source_path() / "include";
}

url openssl::source_url()
{
	return
		"https://www.openssl.org/source/"
		"openssl-" + version() + ".tar.gz";
}

std::vector<std::string> openssl::output_names()
{
	return
	{
		"libcrypto-" + version_no_minor_underscores() + "-x64",
		"libssl-" + version_no_minor_underscores() + "-x64"
	};
}

std::smatch openssl::parse_version()
{
	// 1.1.1d
	// everything but 1 is optional
	std::regex re(R"((\d+)(?:\.(\d+)(?:\.(\d+)([a-zA-Z]+)?)?)?)");
	std::smatch m;

	if (!std::regex_match(version(), m, re))
		bail_out("bad openssl version '" + version() + "'");

	return m;
}

std::string openssl::version_no_tags()
{
	auto m = parse_version();

	// up to 4 so the tag is skipped if present
	const std::size_t count = std::min<std::size_t>(m.size(), 4);

	std::string s;
	for (std::size_t i=1; i<count; ++i)
	{
		if (!s.empty())
			s += ".";

		s += m[i].str();
	}

	return s;
}

std::string openssl::version_no_minor_underscores()
{
	auto m = parse_version();

	if (m.size() == 2)
		return m[1].str();
	else
		return m[1].str() + "_" + m[2].str();
}

}	// namespace
