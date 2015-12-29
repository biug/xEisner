#include <fstream>

#include "emptyeisnergc3rd_weight.h"
#include "common/token/word.h"
#include "common/token/pos.h"
#include "common/token/emp.h"

namespace emptyeisnergc3rd {
	Weight::Weight(const std::string & sRead, const std::string & sRecord) :
		WeightBase(sRead, sRecord),
		// first order
		m_mapPw("m_mapPw"),
		m_mapPp("m_mapPp"),
		m_mapPwp("m_mapPwp"),
		m_mapCw("m_mapCw"),
		m_mapCp("m_mapCp"),
		m_mapCwp("m_mapCwp"),
		m_mapPwpCwp("m_mapPwpCwp"),
		m_mapPpCwp("m_mapPpCwp"),
		m_mapPwpCp("m_mapPwpCp"),
		m_mapPwCwp("m_mapPwCwp"),
		m_mapPwpCw("m_mapPwpCw"),
		m_mapPwCw("m_mapPwCw"),
		m_mapPpCp("m_mapPpCp"),
		m_mapPpBpCp("m_mapPpBpCp"),
		m_mapPpPp1Cp_1Cp("m_mapPpPp1Cp_1Cp"),
		m_mapPp_1PpCp_1Cp("m_mapPp_1PpCp_1Cp"),
		m_mapPpPp1CpCp1("m_mapPpPp1CpCp1"),
		m_mapPp_1PpCpCp1("m_mapPp_1PpCpCp1"),
		// second order
		m_mapC1pC2p("m_mapC1pC2p"),
		m_mapPpC1pC2p("m_mapPpC1pC2p"),
		m_mapC1wC2w("m_mapC1wC2w"),
		m_mapC1wC2p("m_mapC1wC2p"),
		m_mapC2wC1p("m_mapC2wC1p"),
		// third order
		m_mapPwC1pC2pC3p("m_mapPwC1pC2pC3p"),
		m_mapC1wPpC2pC3p("m_mapC1wPpC2pC3p"),
		m_mapC2wPpC1pC3p("m_mapC2wPpC1pC3p"),
		m_mapC3wPpC1pC2p("m_mapC3wPpC1pC2p"),
		m_mapPwC1wC2pC3p("m_mapPwC1wC2pC3p"),
		m_mapPwC2wC1pC3p("m_mapPwC2wC1pC3p"),
		m_mapPwC3wC1pC2p("m_mapPwC3wC1pC2p"),
		m_mapC1wC2wPpC3p("m_mapC1wC2wPpC3p"),
		m_mapC1wC3wPpC2p("m_mapC1wC3wPpC2p"),
		m_mapC2wC3wPpC1p("m_mapC2wC3wPpC1p"),
		m_mapPpC1pC2pC3p("m_mapPpC1pC2pC3p"),
		m_mapC1wC2pC3p("m_mapC1wC2pC3p"),
		m_mapC2wC1pC3p("m_mapC2wC1pC3p"),
		m_mapC3wC1pC2p("m_mapC3wC1pC2p"),
		m_mapC1wC2wC3p("m_mapC1wC2wC3p"),
		m_mapC1wC3wC2p("m_mapC1wC3wC2p"),
		m_mapC2wC3wC1p("m_mapC2wC3wC1p"),
		m_mapC1pC2pC3p("m_mapC1pC2pC3p"),
		m_mapC1wC3w("m_mapC1wC3w"),
		m_mapC1wC3p("m_mapC1wC3p"),
		m_mapC3wC1p("m_mapC3wC1p"),
		m_mapC1pC3p("m_mapC1pC3p"),
		// grand feature
		m_mapGpPpCp("m_mapGpPpCp"),
		m_mapGpCp("m_mapGpCp"),
		m_mapGwCw("m_mapGwCw"),
		m_mapGwCp("m_mapGwCp"),
		m_mapCwGp("m_mapCwGp"),
		// grand sibling feature
		m_mapGwPpCpC2p("m_mapGwPpCpC2p"),
		m_mapPwGpCpC2p("m_mapPwGpCpC2p"),
		m_mapCwGpPpC2p("m_mapCwGpPpC2p"),
		m_mapC2wGpPpCp("m_mapC2wGpPpCp"),
		m_mapGwPwCpC2p("m_mapGwPwCpC2p"),
		m_mapGwCwPpC2p("m_mapGwCwPpC2p"),
		m_mapGwC2wPpCp("m_mapGwC2wPpCp"),
		m_mapPwCwGpC2p("m_mapPwCwGpC2p"),
		m_mapPwC2wGpCp("m_mapPwC2wGpCp"),
		m_mapCwC2wGpPp("m_mapCwC2wGpPp"),
		m_mapGpPpCpC2p("m_mapGpPpCpC2p"),
		m_mapGwCpC2p("m_mapGwCpC2p"),
		m_mapCwGpC2p("m_mapCwGpC2p"),
		m_mapC2wGpCp("m_mapC2wGpCp"),
		m_mapGwCwC2p("m_mapGwCwC2p"),
		m_mapGwC2wCp("m_mapGwC2wCp"),
		m_mapCwC2wGp("m_mapCwC2wGp"),
		m_mapGpCpC2p("m_mapGpCpC2p"),
		// grand tri-sibling feature
		m_mapGwPpCpC2pC3p("m_mapGwPpCpC2pC3p"),
		m_mapPwGpCpC2pC3p("m_mapPwGpCpC2pC3p"),
		m_mapCwGpPpC2pC3p("m_mapCwGpPpC2pC3p"),
		m_mapC2wGpPpCpC2p("m_mapC2wGpPpCpC2p"),
		m_mapC3wGpPpCpC2p("m_mapC3wGpPpCpC2p"),
		m_mapGpPpCpC2pC3p("m_mapGpPpCpC2pC3p"),
		m_mapGwCpC2pC3p("m_mapGwCpC2pC3p"),
		m_mapCwGpC2pC3p("m_mapCwGpC2pC3p"),
		m_mapC2wGpCpC3p("m_mapC2wGpCpC3p"),
		m_mapC3wGpCpC2p("m_mapC3wGpCpC2p"),
		m_mapGpCpC2pC3p("m_mapGpCpC2pC3p")
	{
		loadScores();
		std::cout << "load complete." << std::endl;
	}

	Weight::~Weight() = default;

	void Weight::loadScores() {

		if (m_sReadPath.empty()) {
			return;
		}
		std::ifstream input(m_sReadPath);
		if (!input) {
			return;
		}

		input >> TWord::getTokenizer();

		input >> TPOSTag::getTokenizer();

		input >> TEmptyTag::getTokenizer();

		input >> m_mapPw;
		input >> m_mapPp;
		input >> m_mapPwp;
		input >> m_mapCw;
		input >> m_mapCp;
		input >> m_mapCwp;
		input >> m_mapPwpCwp;
		input >> m_mapPpCwp;
		input >> m_mapPwpCp;
		input >> m_mapPwCwp;
		input >> m_mapPwpCw;
		input >> m_mapPwCw;
		input >> m_mapPpCp;
		input >> m_mapPpBpCp;
		input >> m_mapPpPp1Cp_1Cp;
		input >> m_mapPp_1PpCp_1Cp;
		input >> m_mapPpPp1CpCp1;
		input >> m_mapPp_1PpCpCp1;

		input >> m_mapC1pC2p;
		input >> m_mapPpC1pC2p;
		input >> m_mapC1wC2w;
		input >> m_mapC1wC2p;
		input >> m_mapC2wC1p;

		input >> m_mapPwC1pC2pC3p;
		input >> m_mapC1wPpC2pC3p;
		input >> m_mapC2wPpC1pC3p;
		input >> m_mapC3wPpC1pC2p;
		input >> m_mapPwC1wC2pC3p;
		input >> m_mapPwC2wC1pC3p;
		input >> m_mapPwC3wC1pC2p;
		input >> m_mapC1wC2wPpC3p;
		input >> m_mapC1wC3wPpC2p;
		input >> m_mapC2wC3wPpC1p;
		input >> m_mapPpC1pC2pC3p;
		input >> m_mapC1wC2pC3p;
		input >> m_mapC2wC1pC3p;
		input >> m_mapC3wC1pC2p;
		input >> m_mapC1wC2wC3p;
		input >> m_mapC1wC3wC2p;
		input >> m_mapC2wC3wC1p;
		input >> m_mapC1pC2pC3p;
		input >> m_mapC1wC3w;
		input >> m_mapC1wC3p;
		input >> m_mapC3wC1p;
		input >> m_mapC1pC3p;

		input >> m_mapGpPpCp;
		input >> m_mapGpCp;
		input >> m_mapGwCw;
		input >> m_mapGwCp;
		input >> m_mapCwGp;

		input >> m_mapGwPpCpC2p;
		input >> m_mapPwGpCpC2p;
		input >> m_mapCwGpPpC2p;
		input >> m_mapC2wGpPpCp;
		input >> m_mapGwPwCpC2p;
		input >> m_mapGwCwPpC2p;
		input >> m_mapGwC2wPpCp;
		input >> m_mapPwCwGpC2p;
		input >> m_mapPwC2wGpCp;
		input >> m_mapCwC2wGpPp;
		input >> m_mapGpPpCpC2p;
		input >> m_mapGwCpC2p;
		input >> m_mapCwGpC2p;
		input >> m_mapC2wGpCp;
		input >> m_mapGwCwC2p;
		input >> m_mapGwC2wCp;
		input >> m_mapCwC2wGp;
		input >> m_mapGpCpC2p;

		input >> m_mapGwPpCpC2pC3p;
		input >> m_mapPwGpCpC2pC3p;
		input >> m_mapCwGpPpC2pC3p;
		input >> m_mapC2wGpPpCpC2p;
		input >> m_mapC3wGpPpCpC2p;
		input >> m_mapGpPpCpC2pC3p;
		input >> m_mapGwCpC2pC3p;
		input >> m_mapCwGpC2pC3p;
		input >> m_mapC2wGpCpC3p;
		input >> m_mapC3wGpCpC2p;
		input >> m_mapGpCpC2pC3p;

		input.close();
	}

	void Weight::saveScores() const {

		if (m_sRecordPath.empty()) {
			return;
		}
		std::ofstream output(m_sRecordPath);
		if (!output) {
			return;
		}

		output << TWord::getTokenizer();

		output << TPOSTag::getTokenizer();

		output << TEmptyTag::getTokenizer();

		output << m_mapPw;
		output << m_mapPp;
		output << m_mapPwp;
		output << m_mapCw;
		output << m_mapCp;
		output << m_mapCwp;
		output << m_mapPwpCwp;
		output << m_mapPpCwp;
		output << m_mapPwpCp;
		output << m_mapPwCwp;
		output << m_mapPwpCw;
		output << m_mapPwCw;
		output << m_mapPpCp;
		output << m_mapPpBpCp;
		output << m_mapPpPp1Cp_1Cp;
		output << m_mapPp_1PpCp_1Cp;
		output << m_mapPpPp1CpCp1;
		output << m_mapPp_1PpCpCp1;

		output << m_mapC1pC2p;
		output << m_mapPpC1pC2p;
		output << m_mapC1wC2w;
		output << m_mapC1wC2p;
		output << m_mapC2wC1p;

		output << m_mapPwC1pC2pC3p;
		output << m_mapC1wPpC2pC3p;
		output << m_mapC2wPpC1pC3p;
		output << m_mapC3wPpC1pC2p;
		output << m_mapPwC1wC2pC3p;
		output << m_mapPwC2wC1pC3p;
		output << m_mapPwC3wC1pC2p;
		output << m_mapC1wC2wPpC3p;
		output << m_mapC1wC3wPpC2p;
		output << m_mapC2wC3wPpC1p;
		output << m_mapPpC1pC2pC3p;
		output << m_mapC1wC2pC3p;
		output << m_mapC2wC1pC3p;
		output << m_mapC3wC1pC2p;
		output << m_mapC1wC2wC3p;
		output << m_mapC1wC3wC2p;
		output << m_mapC2wC3wC1p;
		output << m_mapC1pC2pC3p;
		output << m_mapC1wC3w;
		output << m_mapC1wC3p;
		output << m_mapC3wC1p;
		output << m_mapC1pC3p;

		output << m_mapGpPpCp;
		output << m_mapGpCp;
		output << m_mapGwCw;
		output << m_mapGwCp;
		output << m_mapCwGp;

		output << m_mapGwPpCpC2p;
		output << m_mapPwGpCpC2p;
		output << m_mapCwGpPpC2p;
		output << m_mapC2wGpPpCp;
		output << m_mapGwPwCpC2p;
		output << m_mapGwCwPpC2p;
		output << m_mapGwC2wPpCp;
		output << m_mapPwCwGpC2p;
		output << m_mapPwC2wGpCp;
		output << m_mapCwC2wGpPp;
		output << m_mapGpPpCpC2p;
		output << m_mapGwCpC2p;
		output << m_mapCwGpC2p;
		output << m_mapC2wGpCp;
		output << m_mapGwCwC2p;
		output << m_mapGwC2wCp;
		output << m_mapCwC2wGp;
		output << m_mapGpCpC2p;

		output << m_mapGwPpCpC2pC3p;
		output << m_mapPwGpCpC2pC3p;
		output << m_mapCwGpPpC2pC3p;
		output << m_mapC2wGpPpCpC2p;
		output << m_mapC3wGpPpCpC2p;
		output << m_mapGpPpCpC2pC3p;
		output << m_mapGwCpC2pC3p;
		output << m_mapCwGpC2pC3p;
		output << m_mapC2wGpCpC3p;
		output << m_mapC3wGpCpC2p;
		output << m_mapGpCpC2pC3p;

		output.close();
	}

	void Weight::computeAverageFeatureWeights(const int & round) {
		m_mapPw.computeAverage(round);
		m_mapPp.computeAverage(round);
		m_mapPwp.computeAverage(round);
		m_mapCw.computeAverage(round);
		m_mapCp.computeAverage(round);
		m_mapCwp.computeAverage(round);
		m_mapPwpCwp.computeAverage(round);
		m_mapPpCwp.computeAverage(round);
		m_mapPwpCp.computeAverage(round);
		m_mapPwCwp.computeAverage(round);
		m_mapPwpCw.computeAverage(round);
		m_mapPwCw.computeAverage(round);
		m_mapPpCp.computeAverage(round);
		m_mapPpBpCp.computeAverage(round);
		m_mapPpPp1Cp_1Cp.computeAverage(round);
		m_mapPp_1PpCp_1Cp.computeAverage(round);
		m_mapPpPp1CpCp1.computeAverage(round);
		m_mapPp_1PpCpCp1.computeAverage(round);

		m_mapC1pC2p.computeAverage(round);
		m_mapPpC1pC2p.computeAverage(round);
		m_mapC1wC2w.computeAverage(round);
		m_mapC1wC2p.computeAverage(round);
		m_mapC2wC1p.computeAverage(round);

		m_mapPwC1pC2pC3p.computeAverage(round);
		m_mapC1wPpC2pC3p.computeAverage(round);
		m_mapC2wPpC1pC3p.computeAverage(round);
		m_mapC3wPpC1pC2p.computeAverage(round);
		m_mapPwC1wC2pC3p.computeAverage(round);
		m_mapPwC2wC1pC3p.computeAverage(round);
		m_mapPwC3wC1pC2p.computeAverage(round);
		m_mapC1wC2wPpC3p.computeAverage(round);
		m_mapC1wC3wPpC2p.computeAverage(round);
		m_mapC2wC3wPpC1p.computeAverage(round);
		m_mapPpC1pC2pC3p.computeAverage(round);
		m_mapC1wC2pC3p.computeAverage(round);
		m_mapC2wC1pC3p.computeAverage(round);
		m_mapC3wC1pC2p.computeAverage(round);
		m_mapC1wC2wC3p.computeAverage(round);
		m_mapC1wC3wC2p.computeAverage(round);
		m_mapC2wC3wC1p.computeAverage(round);
		m_mapC1pC2pC3p.computeAverage(round);
		m_mapC1wC3w.computeAverage(round);
		m_mapC1wC3p.computeAverage(round);
		m_mapC3wC1p.computeAverage(round);
		m_mapC1pC3p.computeAverage(round);

		m_mapGpPpCp.computeAverage(round);
		m_mapGpCp.computeAverage(round);
		m_mapGwCw.computeAverage(round);
		m_mapGwCp.computeAverage(round);
		m_mapCwGp.computeAverage(round);

		m_mapGwPpCpC2p.computeAverage(round);
		m_mapPwGpCpC2p.computeAverage(round);
		m_mapCwGpPpC2p.computeAverage(round);
		m_mapC2wGpPpCp.computeAverage(round);
		m_mapGwPwCpC2p.computeAverage(round);
		m_mapGwCwPpC2p.computeAverage(round);
		m_mapGwC2wPpCp.computeAverage(round);
		m_mapPwCwGpC2p.computeAverage(round);
		m_mapPwC2wGpCp.computeAverage(round);
		m_mapCwC2wGpPp.computeAverage(round);
		m_mapGpPpCpC2p.computeAverage(round);
		m_mapGwCpC2p.computeAverage(round);
		m_mapCwGpC2p.computeAverage(round);
		m_mapC2wGpCp.computeAverage(round);
		m_mapGwCwC2p.computeAverage(round);
		m_mapGwC2wCp.computeAverage(round);
		m_mapCwC2wGp.computeAverage(round);
		m_mapGpCpC2p.computeAverage(round);

		m_mapGwPpCpC2pC3p.computeAverage(round);
		m_mapPwGpCpC2pC3p.computeAverage(round);
		m_mapCwGpPpC2pC3p.computeAverage(round);
		m_mapC2wGpPpCpC2p.computeAverage(round);
		m_mapC3wGpPpCpC2p.computeAverage(round);
		m_mapGpPpCpC2pC3p.computeAverage(round);
		m_mapGwCpC2pC3p.computeAverage(round);
		m_mapCwGpC2pC3p.computeAverage(round);
		m_mapC2wGpCpC3p.computeAverage(round);
		m_mapC3wGpCpC2p.computeAverage(round);
		m_mapGpCpC2pC3p.computeAverage(round);
	}
}