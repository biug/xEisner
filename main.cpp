#include <memory>
#include <cstring>

#include "common/parser/graph_dp/eisner/eisner_run.h"
#include "common/parser/graph_dp/eisner2nd/eisner2nd_run.h"
#include "common/parser/graph_dp/eisner3rd/eisner3rd_run.h"
#include "common/parser/graph_dp/eisnergc/eisnergc_run.h"
#include "common/parser/graph_dp/eisnergc2nd/eisnergc2nd_run.h"
#include "common/parser/graph_dp/eisnergc3rd/eisnergc3rd_run.h"
#include "common/parser/graph_dp/emptyeisner2nd/emptyeisner2nd_run.h"
#include "common/parser/graph_dp/emptyeisner3rd/emptyeisner3rd_run.h"
#include "common/parser/graph_dp/emptyeisnergc2nd/emptyeisnergc2nd_run.h"
#include "common/parser/graph_dp/emptyeisnergc3rd/emptyeisnergc3rd_run.h"

#define	SLASH	"\\"

int main(int argc, char * argv[]) {

	std::unique_ptr<RunBase> run(nullptr);

	if (strcmp(argv[2], "eisner") == 0) {
		run.reset(new eisner::Run());
	}
	else if (strcmp(argv[2], "eisner2nd") == 0) {
		run.reset(new eisner2nd::Run());
	}
	else if (strcmp(argv[2], "eisner3rd") == 0) {
		run.reset(new eisner3rd::Run());
	}
	else if (strcmp(argv[2], "eisnergc") == 0) {
		run.reset(new eisnergc::Run());
	}
	else if (strcmp(argv[2], "eisnergc2nd") == 0) {
		run.reset(new eisnergc2nd::Run());
	}
	else if (strcmp(argv[2], "eisnergc3rd") == 0) {
		run.reset(new eisnergc3rd::Run());
	}
	else if (strcmp(argv[2], "emptyeisner2nd") == 0) {
		run.reset(new emptyeisner2nd::Run());
	}
	else if (strcmp(argv[2], "emptyeisner3rd") == 0) {
		run.reset(new emptyeisner3rd::Run());
	}
	else if (strcmp(argv[2], "emptyeisnergc2nd") == 0) {
		run.reset(new emptyeisnergc2nd::Run());
	}
	else if (strcmp(argv[2], "emptyeisnergc3rd") == 0) {
		run.reset(new emptyeisnergc3rd::Run());
	}

	if (strcmp(argv[1], "goldtest") == 0) {
		run->goldtest(argv[3], argv[4]);
	}
	else if (strcmp(argv[1], "train") == 0) {

		int iteration = std::atoi(argv[5]);

		std::string current_feature;
		std::string next_feature;

		current_feature = next_feature = argv[4];
		next_feature =
				next_feature.find('#') == std::string::npos ?
						next_feature.substr(0, next_feature.rfind(SLASH) + strlen(SLASH)) + argv[2] + "1.feat" :
						next_feature.substr(0, next_feature.find('#')).substr(0, next_feature.substr(0, next_feature.find('#')).rfind(SLASH) + strlen(SLASH)) +
						argv[2] + "1.feat" + "#" + next_feature.substr(next_feature.find('#') + 1);

		for (int i = 0; i < iteration; ++i) {
//			std::cout << current_feature << std::endl << next_feature << std::endl;
			run->train(argv[3], current_feature, next_feature);
			current_feature = next_feature;
			next_feature = next_feature.find('#') == std::string::npos ?
					next_feature.substr(0, next_feature.rfind(argv[2]) + strlen(argv[2])) + std::to_string(i + 2) + ".feat" :
					next_feature.substr(0, next_feature.find('#')).substr(0, next_feature.substr(0, next_feature.find('#')).rfind(argv[2]) + strlen(argv[2])) +
					std::to_string(i + 2) + ".feat" + "#" + next_feature.substr(next_feature.find('#') + 1);
		}
	}
	else if (strcmp(argv[1], "parse") == 0) {
		run->parse(argv[3], argv[5], argv[4]);
	}
}
