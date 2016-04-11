#include <mex.h>
#include <math.h>
#include <stdio.h>
#include <locale.h>
#include "mrq.h"

/* [mrq] = import_mrq(path)

  Inputs:
   path:        1d vec    real

  Outputs:
   mrq:         struct array
*/

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define mxGetL(pr) max(mxGetN(pr), mxGetM(pr))

mxArray* mrq_entry_to_mx(mrq_entry* src) {
  const char* fields[5] = {"nhop", "fs", "f0", "timestamp", "modified"};
  mxArray* ret = mxCreateStructMatrix(1, 1, 5, fields);
  mxSetFieldByNumber(ret, 0, 0, mxCreateScalarDouble(src -> nhop));
  mxSetFieldByNumber(ret, 0, 1, mxCreateScalarDouble(src -> fs));
  mxSetFieldByNumber(ret, 0, 3, mxCreateScalarDouble(src -> timestamp));
  mxSetFieldByNumber(ret, 0, 4, mxCreateScalarDouble(src -> modified));
  mxArray* f0 = mxCreateDoubleMatrix(src -> nf0, 1, mxREAL);
  double* f0_ = mxGetPr(f0);
  for(int i = 0; i < src -> nf0; i ++)
    f0_[i] = src -> f0[i];
  mxSetFieldByNumber(ret, 0, 2, f0);
  return ret;
}

typedef struct mrq_node {
  struct mrq_node* next;
  mrq_entry* entry;
  wchar_t* filename;
} mrq_node;

typedef struct {
  mrq_node* first;
  mrq_node* top;
  int size;
} mrq_list;

void mrq_list_cpyinsert(mrq_list* dst, mrq_entry* src, const wchar_t* filename) {
  mrq_entry* cpy = create_mrq_entry(src -> nf0);
  cpy -> nhop = src -> nhop;
  cpy -> fs = src -> fs;
  cpy -> timestamp = src -> timestamp;
  cpy -> modified = src -> modified;
  for(int i = 0; i < src -> nf0; i ++)
    cpy -> f0[i] = src -> f0[i];
  mrq_node* node = malloc(sizeof(mrq_node));
  node -> next = NULL;
  node -> entry = cpy;
  node -> filename = wcsdup(filename);
  if(dst -> first == NULL)
    dst -> first = node;
  else
    dst -> top -> next = node;
  dst -> top = node;
  dst -> size ++;
}

void mrq_node_delete(mrq_node* dst) {
  delete_mrq_entry(dst -> entry);
  free(dst -> filename);
  if(dst -> next != NULL)
    mrq_node_delete(dst -> next);
  free(dst);
}

int load_entry(const wchar_t* filename, mrq_entry* src, void* env) {
  if(wcslen(filename) == 0) return 1;
  mrq_list* dst = (mrq_list*)env;
  mrq_list_cpyinsert(dst, src, filename);
  return 1;
}

void mexFunction(int nlhs, mxArray *plhs[],
                 int nrhs, const mxArray *prhs[])
{
  if(nrhs != 1)
    mexErrMsgTxt("Incorrect number of inputs.");
  if(nlhs != 1)
    mexErrMsgTxt("Incorrect number of outputs.");

  char path[512];
  mwSize Npath = mxGetL(prhs[0]);
  mxChar *mxpath = mxGetChars(prhs[0]);

  for(int i = 0; i < Npath; i ++)
    path[i] = mxpath[i];
  path[Npath] = '\0';

  wchar_t wcspath[512];
  setlocale(LC_ALL, "ja_JP.utf8");
  mbstowcs(wcspath, path, 512);

  FILE* mrqfp = mrq_open(wcspath, L"r");   
  if(mrqfp == NULL)
    mexErrMsgTxt("Cannot open the given file.");

  mrq_list content;
  content.first = NULL;
  content.top = NULL;
  content.size = 0;
  mrq_enumerate(mrqfp, load_entry, & content);
  
  const char* fields[2] = {"filename", "entry"};
  mxArray* table = mxCreateStructMatrix(content.size, 1, 2, fields);
  mrq_node* curr = content.first;
  for(int i = 0; i < content.size; i ++, curr = curr -> next) {
    char mbsfilename[512];
    int nfilename = wcstombs(mbsfilename, curr -> filename, 512);
    char* mxfilename = mxCalloc(nfilename + 1, 1);
    for(int j = 0; j < nfilename + 1; j ++) mxfilename[j] = mbsfilename[j];
    mxSetFieldByNumber(table, i, 0, mxCreateString(mxfilename));
    mxSetFieldByNumber(table, i, 1, mrq_entry_to_mx(curr -> entry));
  }
  
  plhs[0] = table;
  
  mrq_node_delete(content.first);
  fclose(mrqfp);
}

