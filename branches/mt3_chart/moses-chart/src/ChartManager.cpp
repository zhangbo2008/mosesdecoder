
#include <stdio.h>
#include "ChartManager.h"
#include "ChartCell.h"
#include "ChartHypothesis.h"
#include "ChartTrellisPath.h"
#include "ChartTrellisPathList.h"
#include "ChartTrellisPathCollection.h"
#include "../../moses/src/StaticData.h"

using namespace std;
using namespace Moses;

namespace MosesChart
{

Manager::Manager(InputType const& source)
:m_source(source)
,m_hypoStackColl(source)
,m_transOptColl(source, StaticData::Instance().GetDecodeGraphList(), m_hypoStackColl)
{
	const StaticData &staticData = StaticData::Instance();
	staticData.InitializeBeforeSentenceProcessing(source);
}

Manager::~Manager()
{
	StaticData::Instance().CleanUpAfterSentenceProcessing();
}

void Manager::ProcessSentence()
{
	TRACE_ERR("Translating: " << m_source << endl);

	const StaticData &staticData = StaticData::Instance();
	staticData.ResetSentenceStats(m_source);

	TRACE_ERR("Decoding: " << endl);
	//Hypothesis::ResetHypoCount();

	// MAIN LOOP
	size_t size = m_source.GetSize();
	for (size_t width = 1; width <= size; ++width)
	{
		for (size_t startPos = 0; startPos <= size-width; ++startPos)
		{
			size_t endPos = startPos + width - 1;

			// create trans opt
			m_transOptColl.CreateTranslationOptionsForRange(startPos, endPos);

			// decode
			WordsRange range(startPos, endPos);
			ChartCell &cell = m_hypoStackColl.Get(range);

			cell.ProcessSentence(m_transOptColl.GetTranslationOptionList(range)
														,m_hypoStackColl);
			cell.PruneToSize(cell.GetMaxHypoStackSize());
			cell.CleanupArcList();
			cell.SortHypotheses();

			TRACE_ERR(range << " = " << cell.GetSize() << endl);
		}
	}

	cerr << "Num of hypo = " << Hypothesis::GetHypoCount() << endl;
}

const Hypothesis *Manager::GetBestHypothesis() const
{
	size_t size = m_source.GetSize();

	if (size == 0) // empty source
		return NULL;
	else
	{
		WordsRange range(0, size-1);
		const ChartCell &lastCell = m_hypoStackColl.Get(range);
		return lastCell.GetBestHypothesis();
	}
}

void Manager::CalcNBest(size_t count, TrellisPathList &ret,bool onlyDistinct) const
{
	size_t size = m_source.GetSize();
	if (count == 0 || size == 0)
		return;
	
	TrellisPathCollection contenders;
	set<Phrase> distinctHyps;

	// add all pure paths
	WordsRange range(0, size-1);
	const ChartCell &lastCell = m_hypoStackColl.Get(range);
	const Hypothesis *hypo = lastCell.GetBestHypothesis();
	
	if (hypo == NULL)
	{ // no hypothesis
		return;
	}

	MosesChart::TrellisPath *purePath = new TrellisPath(hypo);
	contenders.Add(purePath);

	// factor defines stopping point for distinct n-best list if too many candidates identical
	size_t nBestFactor = StaticData::Instance().GetNBestFactor();
  if (nBestFactor < 1) nBestFactor = 1000; // 0 = unlimited

	// MAIN loop
	for (size_t iteration = 0 ; (onlyDistinct ? distinctHyps.size() : ret.GetSize()) < count && contenders.GetSize() > 0 && (iteration < count * nBestFactor) ; iteration++)
	{
		// get next best from list of contenders
		TrellisPath *path = contenders.pop();
		assert(path);

		// create deviations from current best
		path->CreateDeviantPaths(contenders);		

		if(onlyDistinct)
		{
			Phrase tgtPhrase = path->GetOutputPhrase();
			if (distinctHyps.insert(tgtPhrase).second) 
        ret.Add(path);
			else
				delete path;

			const size_t nBestFactor = StaticData::Instance().GetNBestFactor();
			if (nBestFactor > 0)
				contenders.Prune(count * nBestFactor);
		}
		else 
    {
		  ret.Add(path);
			contenders.Prune(count);
    }
	}
}

void Manager::CalcDecoderStatistics() const
{
}

} // namespace

