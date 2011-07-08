//-----------------------------------------------
// Copyright 2011 Wellcome Trust Sanger Institute
// Written by Jared Simpson (js18@sanger.ac.uk)
// Released under the GPL
//-----------------------------------------------
//
// MultiAlignment - Class constructing a multiple
// alignment between a root sequence and a set of sequences
//
#include <iostream>
#include <algorithm>
#include <map>
#include <limits>
#include "MultiAlignment.h"

struct CigarIter
{
    const MAlignData* pData;
    int symIdx; // the index of the cigar string we are parsing
    int baseIdx; // the index of the current base 

    char updateAndEmit(char updateMode)
    {
        char cigSym = getCigarSymbol();
        char outBase;

        if(updateMode == 'I')
        {
            if(cigSym == 'I')
            {
                symIdx += 1;
                outBase = '-';
            }
            else if(cigSym == 'D')
            {
                assert(false);
            }
            else
            {
                symIdx += 1;
                outBase = getOutputBase(cigSym);
                if(outBase != '.' && outBase != '-')
                    baseIdx += 1;
            }
        }
        else if(updateMode == 'D')
        {
            if(cigSym == 'D')
            {
                symIdx += 1;
                outBase = getOutputBase(cigSym);
                if(outBase != '.')
                    baseIdx += 1;
            }
            else if(cigSym == 'I')
            {
                outBase = '-';
            }
            else if(cigSym == 'S')
            {
                outBase = '.';
            }
            else
            {
                outBase = '-';
                //symIdx += 1;
            }
        }
        else
        {
            symIdx += 1;
            outBase = getOutputBase(cigSym);
            if(outBase != '.')
                baseIdx += 1;
        }

        return outBase;
    }

    char getCigarSymbol()
    {
        if(symIdx >= (int)pData->expandedCigar.size())
            return 'S';
        return pData->expandedCigar[symIdx];
    }

    char getOutputBase(char cigSym)
    {
        if(cigSym == 'S')
            return '.';
        else
            return pData->str[baseIdx];
    }

    static bool sortPosition(const CigarIter& a, const CigarIter& b)
    {
        return a.pData->position < b.pData->position;
    }
};
typedef std::vector<CigarIter> CigarIterVector;

MultiAlignment::MultiAlignment(std::string rootStr, const MAlignDataVector& inData)
{
    m_verbose = 0;
    // Build a padded multiple alignment from the pairwise alignments to the root
    CigarIterVector iterVec;

    // Create entry for the root
    MAlignData rootData;
    rootData.str = rootStr;
    rootData.expandedCigar = std::string(rootStr.size(), 'M');
    rootData.position = 0;
    rootData.name = "root";

    m_alignData.push_back(rootData);
    m_alignData.insert(m_alignData.end(), inData.begin(), inData.end());

    CigarIter tmp = {&rootData, 0, 0};
    iterVec.push_back(tmp);

    if(m_verbose > 1)
        printf("%d\t%s\n", 0, tmp.pData->expandedCigar.c_str());

    for(size_t i = 0; i < inData.size(); ++i)
    {
        CigarIter iter = {&inData[i], 0, 0};
        iterVec.push_back(iter);
        if(m_verbose > 1)
            printf("%zu\t%s\n", i+1, iter.pData->expandedCigar.c_str());
    }

    // Build the padded strings by inserting '-' as necessary
    bool done = false;
    while(!done)
    {
        // Check if any strings have a deletion or insertion at this base
        bool hasDel = false;
        bool hasInsert = false;
        bool hasMatch = false;

        for(size_t i = 0; i < iterVec.size(); ++i)
        {
            char sym = iterVec[i].getCigarSymbol();
            if(sym == 'D')
                hasDel = true;
            else if(sym == 'I')
                hasInsert = true;
            else if(sym == 'M')
                hasMatch = true;
        }

        done = !hasDel && !hasInsert && !hasMatch;
        if(done)
            break;

        char updateMode = hasDel ? 'D' : (hasInsert ? 'I' : 'M');
        for(size_t i = 0; i < iterVec.size(); ++i)
        {
            char outSym = iterVec[i].updateAndEmit(updateMode);
            m_alignData[i].padded.append(1,outSym);
        }
    }
}

// 
size_t MultiAlignment::getIdxByName(const std::string& name) const
{
    size_t max = std::numeric_limits<size_t>::max();
    size_t idx = max;
    for(size_t i = 0; i < m_alignData.size(); ++i)
    {
        if(m_alignData[i].name == name)
        {
            if(idx == max)
            {
                idx = i;
            }
            else
            {
                std::cerr << "Error in MultiAlignment::getIdxByName: duplicate rows found for " << name << "\n";
                exit(EXIT_FAILURE);
            }
        }
    }

    if(idx == max)
    {
        std::cerr << "Error in MultiAlignment::getIdxByName: row not found for " << name << "\n";
        exit(EXIT_FAILURE);
    }
    return idx;
}

//
size_t MultiAlignment::getNumColumns() const
{
    assert(!m_alignData.empty());
    return m_alignData.front().padded.size();
}

//
char MultiAlignment::getSymbol(size_t rowIdx, size_t colIdx) const
{
    assert(rowIdx < m_alignData.size());
    assert(colIdx < m_alignData[rowIdx].padded.size());
    return m_alignData[rowIdx].padded[colIdx];
}

size_t MultiAlignment::getBaseIdx(size_t rowIdx, size_t colIdx) const
{
    assert(rowIdx < m_alignData.size());
    assert(colIdx < m_alignData[rowIdx].padded.size());

    // Convert the column index to the baseIndex in the sequence at

    // Ensure this is a real base on the target string.
    assert(getSymbol(rowIdx, colIdx) != '-');

    // Substract the number of padding charcters from the column index to get the 
    // base index
    size_t padSyms = 0;
    for(size_t i = 0; i < colIdx; ++i)
    {
        if(getSymbol(rowIdx, i) == '-')
            padSyms += 1;
    }
    assert(padSyms < colIdx);
    return colIdx - padSyms;
}


//
std::string MultiAlignment::getPaddedSubstr(size_t rowIdx, size_t start, size_t length) const
{
    assert(rowIdx < m_alignData.size());
    assert(start < m_alignData[rowIdx].padded.size());
    assert(start + length < m_alignData[rowIdx].padded.size());
    return m_alignData[rowIdx].padded.substr(start, length);
}

// Generate simple consensus string from the multiple alignment
std::string MultiAlignment::generateConsensus()
{
    assert(!m_alignData.empty() && !m_alignData.front().padded.empty());

    std::string consensus;
    std::string paddedConsensus;

    std::map<char, int> baseMap;

    size_t num_rows = m_alignData.size() - 1; // do not include root
    size_t num_cols = m_alignData.front().padded.size();
    for(size_t i = 0; i < num_cols; ++i)
    {
        baseMap.clear();
        for(size_t j = 1; j < num_rows; ++j)
        {
            char b = m_alignData[j].padded[i];
            if(b != '.')
                baseMap[b]++;
        }

        int max = 0;
        char maxBase = '.';
        for(std::map<char,int>::iterator iter = baseMap.begin(); iter != baseMap.end(); ++iter)
        {
            if(iter->second > max)
            {
                max = iter->second;
                maxBase = iter->first;
            }
        }

        paddedConsensus.append(1,maxBase);

        if(maxBase == '.' && consensus.empty())
            continue; // skip no-call at beginning
        else if(maxBase == '.')
            break; // non-called position in middle of read, stop consensus generation
        else if(maxBase != '-') //
            consensus.append(1, maxBase);
    }

    if(m_verbose > 0)
        print(&paddedConsensus);

    return consensus;
}

//
void MultiAlignment::print(const std::string* pConsensus) const
{
    assert(!m_alignData.empty() && !m_alignData.front().padded.empty());

    // Create a copy of the m_alignData and sort it by position
    MAlignDataVector sortedAlignments = m_alignData;
    std::stable_sort(sortedAlignments.begin(), sortedAlignments.end(), MAlignData::sortPosition);

    size_t len = sortedAlignments[0].padded.size();
    int col_size = 140;
    for(size_t l = 0; l < len; l += col_size)
    {
        if(pConsensus != NULL)
        {
            int diff = pConsensus->size() - l;
            int stop = diff < col_size ? diff : col_size;
            if(stop > 0)
                printf("C\t%s\n", pConsensus->substr(l,stop).c_str());
            else
                printf("C\n");
        }
        
        for(size_t i = 0; i < sortedAlignments.size(); ++i)
        {
            const MAlignData& mad = sortedAlignments[i];
            int diff = mad.padded.size() - l;
            int stop = diff < col_size ? diff : col_size;
            printf("%zu\t%s\t%s\n", i, mad.padded.substr(l, stop).c_str(), mad.name.c_str());
        }
        std::cout << "\n";
    }
}
