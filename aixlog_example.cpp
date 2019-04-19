/***
      __   __  _  _  __     __    ___ 
     / _\ (  )( \/ )(  )   /  \  / __)
    /    \ )(  )  ( / (_/\(  O )( (_ \
    \_/\_/(__)(_/\_)\____/ \__/  \___/

    This file is part of aixlog
    Copyright (C) 2017-2018 Johannes Pohl
    
    This software may be modified and distributed under the terms
    of the MIT license.  See the LICENSE file for details.
***/


#include "aixlog.hpp"
#include "rotation_strategy.hpp"

using namespace std;


int main(int /*argc*/, char** /*argv*/)
{
	AixLog::Log::init<AixLog::SinkCout>(AixLog::Severity::trace, AixLog::Type::normal);
	LOG(INFO) << "Logger with one cout log sink\n";

	AixLog::Log::init(
		{
			/// Log everything into file "all.log"
			make_shared<AixLog::SinkFile>(AixLog::Severity::trace, AixLog::Type::all, "all.log"),
			/// Log everything to file "formatted.log" using new #file and #line types
			make_shared<AixLog::SinkFile>(AixLog::Severity::trace, AixLog::Type::all, "formatted.log", "%Y%m%d %H:%M:%S.#ms #file(#line) [#severity]: #message"),
			/// Log everything to file "strategy.log" using new #file and #line types, as well as a strategy
			make_shared<AixLog::SinkFile>(AixLog::SinkFile::Strategy(AixLog::Severity::trace, AixLog::Type::all, "strategy.log", "%Y%m%d %H:%M:%S.#ms #file(#line) [#severity]: #message")),
			/// Log everything to file "rotation.log" using new #file and #line types, as well as rotation_strategy with a 5k rotation  size and keep 3 rotations
			make_shared<AixLog::SinkFile>(rotation_strategy(AixLog::Severity::trace,
						                                          AixLog::Type::all,
						                                          "rotation.log",
						                                          "%Y%m%d %H:%M:%S.#ms #file(#line) [#severity]: #message",
						                                          true,
						                                          3,
						                                          5*1024)),
			/// Log normal (i.e. non-special) logs to SinkCout
			make_shared<AixLog::SinkCout>(AixLog::Severity::trace, AixLog::Type::normal, "cout: %Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func) #message"),
			/// Log error and higher severity messages to cerr
			make_shared<AixLog::SinkCerr>(AixLog::Severity::error, AixLog::Type::all, "cerr: %Y-%m-%d %H-%M-%S.#ms [#severity] (#tag_func)"),
			/// Log special logs to native log (Syslog on Linux, Android Log on Android, EventLog on Windows, Unified logging on Apple)
			make_shared<AixLog::SinkNative>("aixlog", AixLog::Severity::trace, AixLog::Type::special),
			/// Callback log sink with cout logging in a lambda function
			/// Could also do file logging
			make_shared<AixLog::SinkCallback>(AixLog::Severity::trace, AixLog::Type::all, 
				[](const AixLog::Metadata& metadata, const std::string& message)
				{
					cout << "Callback:\n\tmsg:   " << message << "\n\ttag:   " << metadata.tag.text << "\n\tsever: " << AixLog::Log::to_string(metadata.severity) << " (" << static_cast<int>(metadata.severity) << ")\n\ttype:  " << (metadata.type == AixLog::Type::normal?"normal":"special") << "\n";
					if (metadata.timestamp)
						cout << "\ttime:  " << metadata.timestamp.to_string() << "\n";
					if (metadata.function)
						cout << "\tfunc:  " << metadata.function.name << "\n\tline:  " << metadata.function.line << "\n\tfile:  " << metadata.function.file << "\n";
				}
			)
		}
	);
	/// Log special logs into file "special.log"
	AixLog::Log::instance().add_logsink<AixLog::SinkFile>(AixLog::Severity::trace, AixLog::Type::special, "special.log");

	/// Log with info severity
	LOG(INFO) << "LOG(INFO)\n";
	/// ... with a tag
	LOG(INFO, "guten tag") << "LOG(INFO, \"guten tag\")\n";
	/// ... with an explicit tag (same result as above)
	LOG(INFO) << TAG("guten tag") << "LOG(INFO) << TAG(\"guten tag\")\n";
	/// Log "special" with info severity
	SLOG(INFO) << "SLOG(INFO)\n";
	/// Log with explicit "special" type
	LOG(INFO) << AixLog::Type::special << "LOG(INFO) << AixLog::Type::special\n";
	/// Log with explicit "special" type (now with a macro)
	LOG(INFO) << SPECIAL << "LOG(INFO) << SPECIAL\n";
	/// ... with explicit "special" type and explicit tag
	LOG(INFO) << SPECIAL << TAG("guten tag") << "LOG(INFO) << SPECIAL << TAG(\"guten tag\")\n";

	/// Different log severities
	LOG(FATAL) << "LOG(FATAL)\nLOG(FATAL) Second line\n";
	LOG(FATAL) << TAG("hello") << "LOG(FATAL) << TAG(\"hello\") no line break";
	LOG(FATAL) << "LOG(FATAL) 2 no line break";
	LOG(ERROR) << "LOG(ERROR): change in log-level will add a line break";
	LOG(WARNING) << "LOG(WARNING)";
	LOG(NOTICE) << "LOG(NOTICE)";
	LOG(INFO) << "LOG(INFO)\n";
	LOG(INFO) << TAG("my tag") << "LOG(INFO) << TAG(\"my tag\")\n";
	LOG(DEBUG) << "LOG(DEBUG)\n";
	LOG(TRACE) << "LOG(TRACE)\n";

	/// Conditional logging
	LOG(DEBUG) << COND(1 == 1) << "LOG(DEBUG) will be logged\n";
	LOG(DEBUG) << COND(1 == 2) << "LOG(DEBUG) will not be logged\n";

	/// Colors :-)
	LOG(FATAL) << "LOG(FATAL) " << AixLog::Color::red << "red" << AixLog::Color::none << ", default color\n";
	LOG(FATAL) << "LOG(FATAL) " << COLOR(red) << "red" << COLOR(none) << ", default color (using macros)\n";
	LOG(FATAL) << "LOG(FATAL) " << AixLog::TextColor(AixLog::Color::yellow, AixLog::Color::blue) << "yellow on blue background" << AixLog::Color::none << ", default color\n";
	LOG(FATAL) << "LOG(FATAL) " << COLOR(yellow, blue) << "yellow on blue background" << COLOR(none) << ", default color (using macros)\n";

	AixLog::Severity severity(AixLog::Severity::debug);
	LOG(severity) << "LOG(severity) << severity\n";
}

