/*

  VSEARCH: a versatile open source tool for metagenomics

  Copyright (C) 2014-2015, Torbjorn Rognes, Frederic Mahe and Tomas Flouri
  All rights reserved.

  Contact: Torbjorn Rognes <torognes@ifi.uio.no>,
  Department of Informatics, University of Oslo,
  PO Box 1080 Blindern, NO-0316 Oslo, Norway

  This software is dual-licensed and available under a choice
  of one of two licenses, either under the terms of the GNU
  General Public License version 3 or the BSD 2-Clause License.


  GNU General Public License version 3

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.


  The BSD 2-Clause License

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

*/

#include "vsearch.h"

#include <string>
#include <set>
#include <map>


/*

  Identify sample and otu identifiers in headers, and count
  abundance of the samples in different OTUs.

  http://www.drive5.com/usearch/manual/upp_labels_sample.html
  http://www.drive5.com/usearch/manual/upp_labels_otus.html

  TODO:
  - add relabel @

*/

typedef std::set<std::string> otu_set_t;
typedef std::set<std::string> sample_set_t;
typedef std::pair<std::string, std::string> sample_otu_pair_t;
typedef std::map<sample_otu_pair_t, unsigned long> sample_otu_count_t;
typedef std::map<std::string, std::string> otu_tax_map_t;

struct otutable_s
{
  regex_t regex_sample;
  regex_t regex_otu;
  regex_t regex_tax;
  
  otu_set_t otu_set;
  sample_set_t sample_set;
  sample_otu_count_t sample_otu_count;
  otu_tax_map_t otu_tax_map;
};

static otutable_s otutable;

void otutable_init()
{
  /* compile regular expression matchers */
  
  if (regcomp(&otutable.regex_sample,
              "(^|;)(sample|barcodelabel)=([A-Za-z0-9_=]*)",
              REG_EXTENDED))
    fatal("Compilation of regular expression for sample annotation failed");
  
  if (regcomp(&otutable.regex_otu,
              "(^|;)([Oo][Tt][Uu][A-Za-z0-9_=]*)",
              REG_EXTENDED))
    fatal("Compilation of regular expression for otu annotation failed");
  
  if (regcomp(&otutable.regex_tax,
              "(^|;)tax=([^;]*)($|;)",
              REG_EXTENDED))
    fatal("Compilation of regular expression for taxonomy annotation failed");
}

void otutable_done()
{
  regfree(&otutable.regex_sample);
  regfree(&otutable.regex_otu);
  regfree(&otutable.regex_tax);

  otutable.otu_set.clear();
  otutable.sample_set.clear();
  otutable.sample_otu_count.clear();
}

void otutable_add(char * query_header, char * target_header, long abundance)
{
  /* read sample annotation in query */

  const char legal_in_sample_name[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "_"
    "0123456789";

  regmatch_t pmatch_sample[5];
  int len_sample;
  char * start_sample = query_header;
  if (!regexec(&otutable.regex_sample, query_header, 5, pmatch_sample, 0))
    {
      /* match: use the matching sample name */
      len_sample = pmatch_sample[3].rm_eo - pmatch_sample[3].rm_so;
      start_sample += pmatch_sample[3].rm_so;
    }
  else
    {
      /* no match: use first name in header */
      len_sample = strspn(query_header, legal_in_sample_name);
    }

  char * sample_name = (char *) xmalloc(len_sample+1);
  strncpy(sample_name, start_sample, len_sample);
  sample_name[len_sample] = 0;


  /* read OTU annotation in target */

  regmatch_t pmatch_otu[4];
  int len_otu;
  char * start_otu = target_header;

  if (!regexec(&otutable.regex_otu, target_header, 4, pmatch_otu, 0))
    {
      /* match: use the matching otu name */
      len_otu = pmatch_otu[2].rm_eo - pmatch_otu[2].rm_so;
      start_otu += pmatch_otu[2].rm_so;
    }
  else
    {
      /* no match: use empty string */
      len_otu = 0;
    }
  char * otu_name = (char *) xmalloc(len_otu+1);
  strncpy(otu_name, start_otu, len_otu);
  otu_name[len_otu] = 0;
  

  /* read tax annotation in target */

  regmatch_t pmatch_tax[4];
  int len_tax;
  char * start_tax = target_header;

  if (!regexec(&otutable.regex_tax, target_header, 4, pmatch_tax, 0))
    {
      /* match: use the matching tax name */
      len_tax = pmatch_tax[2].rm_eo - pmatch_tax[2].rm_so;
      start_tax += pmatch_tax[2].rm_so;

      char * tax_name = (char *) xmalloc(len_tax+1);
      strncpy(tax_name, start_tax, len_tax);
      tax_name[len_tax] = 0;
      otutable.otu_tax_map[otu_name] = tax_name;
      free(tax_name);
    }

  /* store data */

  otutable.sample_set.insert(sample_name);
  otutable.otu_set.insert(otu_name);
  otutable.sample_otu_count[sample_otu_pair_t(sample_name,otu_name)]
    += abundance;

  free(otu_name);
  free(sample_name);
}

void otutable_print_otutabout(FILE * fp)
{
  fprintf(fp, "#OTU ID");
  for (sample_set_t::iterator it_sample = otutable.sample_set.begin();
       it_sample != otutable.sample_set.end();
       it_sample++)
    fprintf(fp, "\t%s", it_sample->c_str());
  if (! otutable.otu_tax_map.empty())
    fprintf(fp, "\ttaxonomy");
  fprintf(fp, "\n");

  for (otu_set_t::iterator it_otu = otutable.otu_set.begin();
       it_otu != otutable.otu_set.end();
       it_otu++)
    {
      const char * otu_name = it_otu->c_str();
      fprintf(fp, "%s", otu_name);
      for (sample_set_t::iterator it_sample = otutable.sample_set.begin();
           it_sample != otutable.sample_set.end();
           it_sample++)
        { 
          unsigned long a = 0;
          sample_otu_count_t::iterator it
            = otutable.sample_otu_count.find(sample_otu_pair_t(*it_sample, *it_otu));
          if (it != otutable.sample_otu_count.end())
            a = it->second;
          fprintf(fp, "\t%ld", a);
        }
      if (! otutable.otu_tax_map.empty())
        {
          fprintf(fp, "\t");
          otu_tax_map_t::iterator it
            = otutable.otu_tax_map.find(otu_name);
          if (it != otutable.otu_tax_map.end())
            fprintf(fp, "%s", it->second.c_str());
        }
      fprintf(fp, "\n");
    }
}

void otutable_print_mothur_shared_out(FILE * fp)
{
  fprintf(fp, "label\tGroup\tnumOtus");
  long numotus = 0;
  for (otu_set_t::iterator it_otu = otutable.otu_set.begin();
       it_otu != otutable.otu_set.end();
       it_otu++)
    {
      const char * otu_name = it_otu->c_str();
      fprintf(fp, "\t%s", otu_name);
      numotus++;
    }
  fprintf(fp, "\n");

  for (sample_set_t::iterator it_sample = otutable.sample_set.begin();
       it_sample != otutable.sample_set.end();
       it_sample++)
    {
      fprintf(fp, "vsearch\t%s\t%ld", it_sample->c_str(), numotus);
      
      for (otu_set_t::iterator it_otu = otutable.otu_set.begin();
           it_otu != otutable.otu_set.end();
           it_otu++)
        {
          unsigned long a = 0;
          sample_otu_count_t::iterator it
            = otutable.sample_otu_count.find(sample_otu_pair_t(*it_sample, *it_otu));
          if (it != otutable.sample_otu_count.end())
            a = it->second;

          fprintf(fp, "\t%ld", a);
        }

      fprintf(fp, "\n");
    }
}

void otutable_print_biomout(FILE * fp)
{
  long rows = otutable.otu_set.size();
  long columns = otutable.sample_set.size();

  static time_t time_now = time(0);
  struct tm tm_now;
  localtime_r(& time_now, & tm_now);
  char date[50];
  strftime(date, 50, "%Y-%m-%dT%H:%M:%S", & tm_now);

  fprintf(fp,
          "{\n"
          "\t\"id\":\"%s\",\n"
          "\t\"format\": \"Biological Observation Matrix 1.0\",\n"
          "\t\"format_url\": \"http://biom-format.org/documentation/format_versions/biom-1.0.html\",\n"
          "\t\"type\": \"OTU table\",\n"
          "\t\"generated_by\": \"%s %s\",\n"
          "\t\"date\": \"%s\",\n"
          "\t\"matrix_type\": \"sparse\",\n"
          "\t\"matrix_element_type\": \"int\",\n"
          "\t\"shape\": [%ld,%ld],\n",
          opt_biomout,
          PROG_NAME, PROG_VERSION,
          date,
          rows,
          columns);
  
  fprintf(fp, "\t\"rows\":[");
  for (sample_set_t::iterator it_otu = otutable.otu_set.begin();
       it_otu != otutable.otu_set.end();
       it_otu++)
    {
      if (it_otu != otutable.otu_set.begin())
        fprintf(fp, ",");
      const char * otu_name = it_otu->c_str();
      fprintf(fp, "\n\t\t{\"id\":\"%s\", \"metadata\":", otu_name);
      if (otutable.otu_tax_map.empty())
        fprintf(fp, "null");
      else
        {
          fprintf(fp, "{\"taxonomy\":\"");
          otu_tax_map_t::iterator it
            = otutable.otu_tax_map.find(otu_name);
          if (it != otutable.otu_tax_map.end())
            fprintf(fp, "%s", it->second.c_str());
          fprintf(fp, "\"}");
        }
      fprintf(fp, "}");
    }
  fprintf(fp, "\n");
  fprintf(fp, "\t],\n");

  fprintf(fp, "\t\"columns\":[");
  for (sample_set_t::iterator it_sample = otutable.sample_set.begin();
       it_sample != otutable.sample_set.end();
       it_sample++)
    {
      if (it_sample != otutable.sample_set.begin())
        fprintf(fp, ",");
      fprintf(fp, "\n\t\t{\"id\":\"%s\", \"metadata\":null}", it_sample->c_str());
    }
  fprintf(fp, "\n\t],\n");

  bool first = true;
  fprintf(fp, "\t\"data\": [");
  long otu_no = 0;
  for (otu_set_t::iterator it_otu = otutable.otu_set.begin();
       it_otu != otutable.otu_set.end();
       it_otu++)
    {
      long sample_no = 0;
      for (sample_set_t::iterator it_sample = otutable.sample_set.begin();
           it_sample != otutable.sample_set.end();
           it_sample++)
        { 
          sample_otu_count_t::iterator it
            = otutable.sample_otu_count.find(sample_otu_pair_t(*it_sample, *it_otu));
          if (it != otutable.sample_otu_count.end())
            {
              if (!first)
                fprintf(fp, ",");
              fprintf(fp, "\n\t\t[%ld,%ld,%lu]", otu_no, sample_no, it->second);
              first = false;
            }
          sample_no++;
        }
      otu_no++;
    }
  fprintf(fp, "\n\t]\n");

  fprintf(fp, "}\n");
}
