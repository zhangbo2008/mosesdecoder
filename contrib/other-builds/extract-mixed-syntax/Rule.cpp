/*
 * Rule.cpp
 *
 *  Created on: 20 Feb 2014
 *      Author: hieu
 */

#include <sstream>
#include <algorithm>
#include "Rule.h"
#include "AlignedSentence.h"
#include "ConsistentPhrase.h"
#include "NonTerm.h"
#include "Parameter.h"

using namespace std;

Rule::Rule(const NonTerm &lhsNonTerm, const AlignedSentence &alignedSentence)
:m_lhs(lhsNonTerm)
,m_alignedSentence(alignedSentence)
,m_isValid(true)
,m_canRecurse(true)
{
	CreateSource();
}

Rule::Rule(const Rule &copy, const NonTerm &nonTerm)
:m_lhs(copy.m_lhs)
,m_alignedSentence(copy.m_alignedSentence)
,m_isValid(true)
,m_canRecurse(true)
,m_nonterms(copy.m_nonterms)
{
	m_nonterms.push_back(&nonTerm);
	CreateSource();

}

Rule::~Rule() {
	// TODO Auto-generated destructor stub
}

const ConsistentPhrase &Rule::GetConsistentPhrase() const
{ return m_lhs.GetConsistentPhrase(); }

void Rule::CreateSource()
{
  const NonTerm *cp = NULL;
  size_t nonTermInd = 0;
  if (nonTermInd < m_nonterms.size()) {
	  cp = m_nonterms[nonTermInd];
  }

  for (int sourcePos = m_lhs.GetConsistentPhrase().corners[0];
		  sourcePos <= m_lhs.GetConsistentPhrase().corners[1];
		  ++sourcePos) {

	  const RuleSymbol *ruleSymbol;
	  if (cp && cp->GetConsistentPhrase().corners[0] <= sourcePos && sourcePos <= cp->GetConsistentPhrase().corners[1]) {
		  // replace words with non-term
		  ruleSymbol = cp;
		  sourcePos = cp->GetConsistentPhrase().corners[1];
		  if (m_nonterms.size()) {
			  cp = m_nonterms[nonTermInd];
		  }

		  // move to next non-term
		  ++nonTermInd;
		  cp = (nonTermInd < m_nonterms.size()) ? m_nonterms[nonTermInd] : NULL;
	  }
	  else {
		  // terminal
		  ruleSymbol = m_alignedSentence.GetPhrase(Moses::Input)[sourcePos];
	  }

	  m_source.push_back(ruleSymbol);
  }
}

int Rule::GetNextSourcePosForNonTerm() const
{
	if (m_nonterms.empty()) {
		// no non-terms so far. Can start next non-term on left corner
		return m_lhs.GetConsistentPhrase().corners[0];
	}
	else {
		// next non-term can start just left of previous
		const ConsistentPhrase &cp = m_nonterms.back()->GetConsistentPhrase();
		int nextPos = cp.corners[1] + 1;
		return nextPos;
	}
}

std::string Rule::Debug() const
{
  stringstream out;

  // source
  for (size_t i =  0; i < m_source.size(); ++i) {
	  const RuleSymbol &symbol = *m_source[i];
	  out << symbol.Debug() << " ";
  }

  // target
  out << "||| ";
  for (size_t i =  0; i < m_target.size(); ++i) {
	  const RuleSymbol &symbol = *m_target[i];
	  out << symbol.Debug() << " ";
  }

  out << "||| ";
  Alignments::const_iterator iterAlign;
  for (iterAlign =  m_alignments.begin(); iterAlign != m_alignments.end(); ++iterAlign) {
	  const std::pair<int,int> &alignPair = *iterAlign;
	  out << alignPair.first << "-" << alignPair.second << " ";
  }

  // overall range
  out << "||| LHS=" << m_lhs.Debug();

  return out.str();
}

void Rule::Output(std::ostream &out) const
{
  // source
  for (size_t i =  0; i < m_source.size(); ++i) {
	  const RuleSymbol &symbol = *m_source[i];
	  symbol.Output(out);
	  out << " ";
  }
  m_lhs.Output(out, Moses::Input);

  out << " ||| ";

  // target
  for (size_t i =  0; i < m_target.size(); ++i) {
	  const RuleSymbol &symbol = *m_target[i];
	  symbol.Output(out);
	  out << " ";
  }
  m_lhs.Output(out, Moses::Output);

  out << " ||| ";

  // alignment
  Alignments::const_iterator iterAlign;
  for (iterAlign =  m_alignments.begin(); iterAlign != m_alignments.end(); ++iterAlign) {
	  const std::pair<int,int> &alignPair = *iterAlign;
	  out << alignPair.first << "-" << alignPair.second << " ";
  }
}

void Rule::Prevalidate(const Parameter &params)
{
  // check number of source symbols in rule
  if (m_source.size() >= params.maxSymbolsSource) {
	  m_canRecurse = false;
	  if (m_source.size() > params.maxSymbolsSource) {
		  m_isValid = false;
		  return;
	  }
  }

  // check that last non-term added isn't too small
  if (m_nonterms.size()) {
	  const NonTerm &lastNonTerm = *m_nonterms.back();
	  const ConsistentPhrase &cp = lastNonTerm.GetConsistentPhrase();

	  int sourceWidth = cp.corners[1]  - cp.corners[0] + 1;
	  if (sourceWidth < params.minHoleSource) {
		  m_isValid = false;
		  m_canRecurse = false;
		  return;
	  }
  }

  // check number of non-terms
  int numNonTerms = 0;
  for (size_t i = 0; i < m_source.size(); ++i) {
	  const RuleSymbol *arc = m_source[i];
	  if (arc->IsNonTerm()) {
		  ++numNonTerms;
	  }
  }

  if (numNonTerms >= params.maxNonTerm) {
	  m_canRecurse = false;
	  if (numNonTerms > params.maxNonTerm) {
		  m_isValid = false;
		  return;
	  }
  }

  // check if 2 consecutive non-terms in source
  if (!params.nonTermConsecSource && m_nonterms.size() >= 2) {
	  const NonTerm &lastNonTerm = *m_nonterms.back();
	  const NonTerm &secondLastNonTerm = *m_nonterms[m_nonterms.size() - 2];
	  if (secondLastNonTerm.GetConsistentPhrase().corners[1] + 1 ==
			  lastNonTerm.GetConsistentPhrase().corners[0]) {
		  m_isValid = false;
		  m_canRecurse = false;
		  return;
	  }
  }

  //check to see if it overlaps with any other non-terms
  if (m_nonterms.size() >= 2) {
	  const NonTerm &lastNonTerm = *m_nonterms.back();

	  for (size_t i = 0; i < m_nonterms.size() - 1; ++i) {
		  const NonTerm &otherNonTerm = *m_nonterms[i];
		  bool overlap = lastNonTerm.GetConsistentPhrase().TargetOverlap(otherNonTerm.GetConsistentPhrase());

		  if (overlap) {
			  m_isValid = false;
			  m_canRecurse = false;
			  return;
		  }
	  }
  }

  // check that at least 1 word is aligned
  if (params.requireAlignedWord) {
	  m_isValid = false;
	  for (size_t i = 0; i < m_source.size(); ++i) {
		  const RuleSymbol &symbol = *m_source[i];
		  if (!symbol.IsNonTerm()) {
			  const Word &word = static_cast<const Word&>(symbol);
			  if (word.GetAlignment().size()) {
				  m_isValid = true;
				  break;
			  }
		  }
	  }

	  if (!m_isValid) {
		  m_canRecurse = false;
		  return;
	  }
  }

}

bool CompareTargetNonTerms(const NonTerm *a, const NonTerm *b)
{
	// compare just start target pos
	return a->GetConsistentPhrase().corners[2] < b->GetConsistentPhrase().corners[2];
}

void Rule::CreateTarget(const Parameter &params)
{
  if (!m_isValid) {
	  return;
  }

  vector<const NonTerm*> targetNonTerm(m_nonterms);
  std::sort(targetNonTerm.begin(), targetNonTerm.end(), CompareTargetNonTerms);

  const NonTerm *cp = NULL;
  size_t nonTermInd = 0;
  if (nonTermInd < targetNonTerm.size()) {
	  cp = targetNonTerm[nonTermInd];
  }

  for (int targetPos = m_lhs.GetConsistentPhrase().corners[2];
		  targetPos <= m_lhs.GetConsistentPhrase().corners[3];
		  ++targetPos) {

	  const RuleSymbol *ruleSymbol;
	  if (cp && cp->GetConsistentPhrase().corners[2] <= targetPos && targetPos <= cp->GetConsistentPhrase().corners[3]) {
		  // replace words with non-term
		  ruleSymbol = cp;
		  targetPos = cp->GetConsistentPhrase().corners[3];
		  if (targetNonTerm.size()) {
			  cp = targetNonTerm[nonTermInd];
		  }

		  // move to next non-term
		  ++nonTermInd;
		  cp = (nonTermInd < targetNonTerm.size()) ? targetNonTerm[nonTermInd] : NULL;
	  }
	  else {
		  // terminal
		  ruleSymbol = m_alignedSentence.GetPhrase(Moses::Output)[targetPos];
	  }

	  m_target.push_back(ruleSymbol);
  }

  CreateAlignments();
}


void Rule::CreateAlignments()
{
	int sourceStart = GetConsistentPhrase().corners[0];
	int targetStart = GetConsistentPhrase().corners[2];

  for (size_t sourcePos = 0; sourcePos < m_source.size(); ++sourcePos) {
	  const RuleSymbol *symbol = m_source[sourcePos];
	  if (!symbol->IsNonTerm()) {
		  // terminals
		  const Word &sourceWord = static_cast<const Word&>(*symbol);
		  const std::set<const Word *> &targetWords = sourceWord.GetAlignment();
		  CreateAlignments(sourcePos, targetWords);
	  }
	  else {
		  // non-terms. same object in both source & target
		  CreateAlignments(sourcePos, symbol);
	  }
  }
}

void Rule::CreateAlignments(int sourcePos, const std::set<const Word *> &targetWords)
{
	std::set<const Word *>::const_iterator iterTarget;
	for (iterTarget = targetWords.begin(); iterTarget != targetWords.end(); ++iterTarget) {
		const Word *targetWord = *iterTarget;
		CreateAlignments(sourcePos, targetWord);
	}
}

void Rule::CreateAlignments(int sourcePos, const RuleSymbol *targetSought)
{
	// should be in target phrase
	for (size_t targetPos = 0; targetPos < m_target.size(); ++targetPos) {
		const RuleSymbol *foundSymbol = m_target[targetPos];
		if (targetSought == foundSymbol) {
			pair<int, int> alignPoint(sourcePos, targetPos);
			m_alignments.insert(alignPoint);
			return;
		}
	}

	throw "not found";
}

