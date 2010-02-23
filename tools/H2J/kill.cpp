#include "kill.hpp"
#include <stdio.h>

list<dep_t> flow_deps, output_deps, merged_deps;

// Forward Function Declarations
int parse_petit_output(std::ifstream &ifs);
int readNextSource(string line, string &source, std::ifstream &ifs);
int readNextDestination(string line, string source, std::ifstream &ifs);
bool isEOR(string line);
void store_dep(list<dep_t> &depList, dep_t dep);
string skipToNext(std::ifstream &ifs);
//void dumpList(list<dep_t> depList);
void mergeLists(void);
void dumpDep(dep_t dep);


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// C++ code

string SetIntersector::itos(int num){
    stringstream out;
    out << num;
    return out.str();
}

void SetIntersector::setFD1(dep_t fd){
    f_dep = fd;
    composed_deps.clear();
    cmpsd_counter = 0;
}

void SetIntersector::setOD(dep_t od){
    o_dep = od;
}
  
void SetIntersector::setFD2(dep_t fd){
    f_dep2 = fd;
}

void SetIntersector::compose_FD2_OD(){
    string cmpsd = "cmpsd"+itos(cmpsd_counter);
    cmpsd += " := "+f_dep2.sets+" compose "+o_dep.sets+";";
    composed_deps.push_back(cmpsd);
    ++cmpsd_counter;
}

string SetIntersector::subtract(){
    stringstream ret_val;
    ret_val << "symbolic ";
    set<string>::iterator sym_itr;
    for(sym_itr=symbolic_vars.begin(); sym_itr != symbolic_vars.end(); ++sym_itr) {
        string sym_var = *sym_itr;
        if( sym_itr!=symbolic_vars.begin() )
            ret_val << ", ";
        ret_val << sym_var;
    }
    ret_val << ";" << endl;

    ret_val << "f1 := " << f_dep.sets << ";" << endl;

    list<string>::iterator cd_itr;
    for(cd_itr=composed_deps.begin(); cd_itr != composed_deps.end(); ++cd_itr) {
        string cd = *cd_itr;
        ret_val << cd <<  endl;
    }

    ret_val << "R := f1";
    for(int i=0; i<cmpsd_counter; i++){
        if( i )
            ret_val << " union ";
        else
            ret_val << " - ( ";

        ret_val << "cmpsd" << i;

        if( i == cmpsd_counter-1 )
            ret_val << " )";
    }
    ret_val << ";" << endl;
    ret_val << "R;" << endl;

    return ret_val.str();
}




////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// C-like code begins
int main(int argc, char **argv){
    char *fName;

    if( argc < 2 ){
        cerr << "Usage: "<< argv[0] << " Pettit_Output_File" << endl;
        exit(-1);
    }

    fName = argv[1];
    ifstream ifs( fName );
    if( !ifs ){
        cerr << "File \""<< fName <<"\" does not exist" << endl;
        exit(-1);
    }

    parse_petit_output(ifs);
    return 0;
}

int parse_petit_output(ifstream &ifs){
    int i;
    string line, source;

    flow_deps.clear();
    output_deps.clear();
    i=0;
    while( getline(ifs,line) ){

        if( readNextSource(line, source, ifs) ){ return -1; }
        line = skipToNext(ifs);
        while( 1 ){
            if( readNextDestination(line, source, ifs) ){ return -1; }
            line = skipToNext(ifs);
            if( line.empty() || line.find("#####") != string::npos ){ break; }
        }
    }

    mergeLists();

    return 0;
}

int readNextSource(string line, string &source, ifstream &ifs){
    unsigned int pos;

    pos = line.find("### SOURCE:");
    while( pos == string::npos ){
        if( !getline(ifs,line) ) return 0;
        pos = line.find("### SOURCE:");
    }

    pos = line.find(":");
    // pos+2 to skip the ":" and the empty space following it
    line = line.substr(pos+2);
    if( line.empty() ){
        cerr << "Empty SOURCE" << endl;
        return -1;
    }else{
        source = line;
//        cout << "New Source Found: "<< source << endl;
    }

    return 0;
}


// Sample format of flow/output dependencies
//
// --> DTSTRF
// flow    10: A(k,k)          -->  16: A(k,k)          (0)             [ M]
// {[k] -> [k,m] : 0 <= k < m < BB}
// exact dd: {[0]}
//
// --> DTSTRF
// output  10: A(k,k)          -->  17: A(k,k)          (0)             [ M]
// {[k] -> [k,m] : 0 <= k < m < BB}
// exact dd: {[0]}
//
int readNextDestination(string line, string source, ifstream &ifs){
    stringstream ss;
    string sink, type, srcLine, junk, dstLine;
//    string sink, type, srcLine, srcArray, junk, dstLine, dstArray;
    dep_t dep;

    // read the sink of this dependency
    unsigned int pos = line.find("===>");
    if( pos != string::npos ){
        pos = line.find(">");
        // pos+2 to skip the ">" and the empty space following it
        line = line.substr(pos+2);
        if( line.empty() ){
            cerr << "Empty sink" << endl;
            return -1;
        }else{
            sink = line;
//            cout << "New Sink Found: "<< source << " --> " << sink << endl;
        }
    }

    // read the details of this dependency
    if( !getline(ifs,line) || isEOR(line) ){ return 0; }

    dep.source = source;
    dep.sink = sink;
    ss << line;
    ss >> type >> srcLine >> dep.srcArray >> junk >> dstLine >> dep.dstArray >> dep.loop ;

    dep.type = type;
    if( dep.loop.find("[") != string::npos ){
        dep.loop.clear();
    }

    // Remove the ":" from the source and destination line.
    pos = srcLine.find(":");
    if( pos != string::npos ){
        dep.srcLine = atoi( srcLine.substr(0,pos).c_str() );
    }
    pos = dstLine.find(":");
    if( pos != string::npos ){
        dep.dstLine = atoi( dstLine.substr(0,pos).c_str() );
    }

    // read the sets of values for the index variables
    if( !getline(ifs,line) || isEOR(line) ){ return 0; }
    dep.sets = line;

    // Store this dependency into the map.
    if( !type.compare("flow") ){
        store_dep(flow_deps, dep);
    }else if( !type.compare("output") ){
        store_dep(output_deps, dep);
    }else{
        cerr << "Unknown type of dependency. ";
        cerr << "Only \"flow\" and \"output\" types are accepted" << endl;
        return -1;
    }

    return 0;
}


void store_dep(list<dep_t> &depList, dep_t dep){
    depList.push_back(dep);
    return;
}

// is this line an End Of Record
bool isEOR(string line){
    unsigned int pos = line.find("#########");
    if( pos != string::npos || line.empty() ){
        return true;
    }
    return false;
}

string skipToNext(ifstream &ifs){
    string line;
    while( 1 ){
        if( !getline(ifs,line) ) return string("");
        unsigned int pos = line.find("===>");
        if( pos != string::npos ){ break; }
        pos = line.find("#######");
        if( pos != string::npos ){ break; }
    }
    return line;
}

string trim(string in){
    int i;
    for(i=0; i<in.length(); ++i){
        if(in[i] != ' ')
            break;
    }
    return in.substr(i);
}

list<string> stringToVarList( string str ){
    list<string> result;
    stringstream ss;

    ss << str;

    while (!ss.eof()) {
        string token;    
        getline(ss, token, ',');
        result.push_back(token);
    }

    return result;
}

void dumpDep(dep_t dep, string iv_set){
    stringstream ss;
    string srcParams, dstParams, junk;
    unsigned int posLB, posRB, posCOL;

    if( iv_set.find("FALSE") != string::npos )
        return;

    // Get the list of formal parameters of the source task (k,m,n,...)
    posLB = dep.source.find("(");
    posRB = dep.source.find(")");
    if( posLB == string::npos || posRB == string::npos){
       cerr << "Malformed dependency source: \"" << dep.source << "\"" << endl; 
       return;
    }
    string srcFrmlStr = dep.source.substr(posLB+1,posRB-posLB-1);
    list<string> srcFormals = stringToVarList( srcFrmlStr );

    // Get the list of formal parameters of the destination task (k,m,n,...)
    posLB = dep.sink.find("(");
    posRB = dep.sink.find(")");
    if( posLB == string::npos || posRB == string::npos){
       cerr << "Malformed dependency sink: \"" << dep.sink << "\"" << endl; 
       return;
    }
    string dstFrmlStr = dep.sink.substr(posLB+1,posRB-posLB-1);
    list<string> dstFormals = stringToVarList( dstFrmlStr );

    // Process the sets of actual parameters
    ss << iv_set;
    ss >> srcParams >> junk >> dstParams;

    // Get the list of actual parameters of the source task
    posLB = srcParams.find("[");
    posRB = srcParams.find("]");
    if( posLB == string::npos || posRB == string::npos){
       cerr << "Malformed set: \"" << iv_set << "\"" << endl; 
       return;
    }
    srcParams = srcParams.substr(posLB+1,posRB-posLB-1);
    list<string> srcActuals = stringToVarList(srcParams);

    // Get the list of actual parameters of the destination task
    posLB = dstParams.find("[");
    posRB = dstParams.find("]");
    if( posLB == string::npos || posRB == string::npos){
       cerr << "Malformed set: \"" << iv_set << "\"" << endl; 
       return;
    }
    dstParams = dstParams.substr(posLB+1,posRB-posLB-1);
    list<string> dstActuals = stringToVarList(dstParams);

    // Get the conditions that Omega told us and clean up the string
    string cond = ss.str();
    posCOL = cond.find(":");
    posRB = cond.find("}");
    if( posCOL == string::npos || posRB == string::npos ){
       cerr << "Malformed conditions: \"" << iv_set << "\"" << endl; 
       return;
    }
    cond = trim(cond.substr(posCOL+1, posRB-posCOL-1));

    // Remove the formals from the dep.sink string
    posLB = dep.sink.find("(");
    if( posLB == string::npos ){
       cerr << "Malformed dependency sink: \"" << dep.sink << "\"" << endl; 
       return;
    }
    string sink = dep.sink.substr(0,posLB);
    

//DEBUG: {[k,k+1,m] -> [k+1,m] : 0 <= k <= m-2 && m < BB}
//DSSSSM(k,n,m)(k,k+1,m) A(m,n) -> A(m,k) DTSTRF(k,m)(k+1,m)  {0 <= k <= m-2 && m < BB}

//DEBUG: {[k,m] -> [k,n,m] : 0 <= k < m,n < BB}
//DTSTRF(k,m)(k,m) IPIV(m,k) -> IPIV(m,k) DSSSSM(k,n,m)(k,n,m)  {0 <= k < m,n < BB}

//    list<string> srcFormals, dstFormals, srcActuals, dstActuals
    if( srcFormals.size() != srcActuals.size() ){
        cerr << "ERROR: source formals != source actuals" << endl;
    }

    // For every source actual that is not the same variable as the formal
    // (i.e. it's an expression) add the condition (formal_variable=actual_expression)
    list<string>::iterator srcF_itr=srcFormals.begin();
    list<string>::iterator srcA_itr=srcActuals.begin();
    for (; srcF_itr!=srcFormals.end(); ++srcF_itr, ++srcA_itr){
        string fParam = *srcF_itr;
        string aParam = *srcA_itr;
        if( fParam.compare(aParam) != 0 ){ // if they are different
            cond = cond.append(" && ("+fParam+"="+aParam+") ");
        }
    }

    list<string> actual_parameter_list;

    // For every destination formal, check if it exists among the source formals.
    // If it doesn't and the corresponding destination actual is not an expression
    // (i.e. the actual is the same as the formal) replace it with a lb..ub expression.
    list<string>::iterator dstF_itr=dstFormals.begin();
    list<string>::iterator dstA_itr=dstActuals.begin();
    for (; dstF_itr!=dstFormals.end(); ++dstF_itr, ++dstA_itr){
        bool found=false;
        string dstfParam = *dstF_itr;
        string dstaParam = *dstA_itr;
        // look through the source formals to see if it's there
        list<string>::iterator srcF_itr=srcFormals.begin();
        for (; srcF_itr!=srcFormals.end(); ++srcF_itr){
            string srcfParam = *srcF_itr;
            if( !srcfParam.compare(dstfParam) ){
                found = true;
                break;
            }
        }
        if( found ){
            actual_parameter_list.push_back(dstaParam);
            continue;
        }
        // if we didn't find the formal, check if the actual is the same as the formal
        if( !dstaParam.compare(dstfParam) ){
            actual_parameter_list.push_back("lb..ub");
        }else{ // in this case it's probably an expression of source actuals
            actual_parameter_list.push_back(dstaParam);
        }
    }

    // iterate over the newly created list of actual destination parameters and
    // create a comma separeted list in a string, so we can print it.
    string dstTaskParams;
    list<string>::iterator a_itr=actual_parameter_list.begin();
    for (; a_itr!=actual_parameter_list.end(); ++a_itr){
        string dstActualParam = *a_itr;
        if( a_itr != actual_parameter_list.begin() ){
            dstTaskParams = dstTaskParams.append(",");
        }
        dstTaskParams = dstTaskParams.append(dstActualParam);
    }


    cout << "  OUT " << dep.srcArray << " -> ";
    cout << dep.dstArray << " ";
// HERE
//    cout << dep.sink << "(" << dstParams << ")  ";
//    cout << sink << "(" << dstParams << ")  ";
    cout << sink << "(" << dstTaskParams << ")  ";
    cout << "{" << cond << "}" << endl;
}


void mergeLists(void){
    bool found;
    list<dep_t>::iterator fd_itr; // flow dep iterator
    list<dep_t>::iterator od_itr; // out dep iterator
    list<dep_t>::iterator fd2_itr; // second flow dep iterator
    set<int> srcSet;
    set<int>::iterator src_itr;
    set<string> fake_it;
    string current_source, prev_source;

    fake_it.insert("BB");
    fake_it.insert("step");
    fake_it.insert("NT");
    fake_it.insert("ip");
    fake_it.insert("proot");
    fake_it.insert("P");
    fake_it.insert("B");

    SetIntersector setIntersector(fake_it);

    // Insert every source of flow deps in a set (srcSet).
    for(fd_itr=flow_deps.begin(); fd_itr != flow_deps.end(); ++fd_itr) {
        found = false;
        dep_t f_dep = *fd_itr;
        srcSet.insert(f_dep.srcLine);
    }

    // For every source of a flow dep
    for (src_itr=srcSet.begin(); src_itr!=srcSet.end(); ++src_itr){
        int source = static_cast<int>(*src_itr);
        list<dep_t> rlvnt_flow_deps;
        // Find all flow deps that flow from this source
        for(fd_itr=flow_deps.begin(); fd_itr != flow_deps.end(); ++fd_itr) {
            dep_t f_dep = static_cast<dep_t>(*fd_itr);
            int fd_srcLine = f_dep.srcLine;
            int fd_dstLine = f_dep.dstLine;
            if( fd_srcLine == source ){
                rlvnt_flow_deps.push_back(f_dep);
                current_source = f_dep.source;
            }
        }

        // Print the source and the parameter space
        if( current_source.compare(prev_source) ){
            if( !prev_source.empty() )
                cout << "}" << endl;
            cout << "\nTASK: " << current_source << " {" << endl;
            prev_source = current_source;
        }
/*
        if( (fd_itr=rlvnt_flow_deps.begin()) != rlvnt_flow_deps.end() ) {
            dep_t f_dep = static_cast<dep_t>(*fd_itr);
            cout << "\nTASK: " << f_dep.source << " {" << endl;
        }
*/
        for(fd_itr=rlvnt_flow_deps.begin(); fd_itr != rlvnt_flow_deps.end(); ++fd_itr) {
            dep_t f_dep = static_cast<dep_t>(*fd_itr);
            int fd_srcLine = f_dep.srcLine;
            int fd_dstLine = f_dep.dstLine;
            setIntersector.setFD1(f_dep);
            // Find and print every output dep that has the same source as this flow
            // dep and its destination is either a) the same as the source, or
            // b) the source of a second flow dep which has the same destination
            // as "fd_dstLine"
            for(od_itr=output_deps.begin(); od_itr != output_deps.end(); ++od_itr) {
                dep_t o_dep = *od_itr;
                int od_srcLine = o_dep.srcLine;
                int od_dstLine = o_dep.dstLine;

                if( fd_srcLine != od_srcLine ){
                    continue;
                }

                setIntersector.setOD(o_dep);

                // case (a)
                if( od_srcLine == od_dstLine ){
                    // Yes, the same as the original fd because the od is (cyclic) on myself.
                    setIntersector.setFD2(f_dep);
                    setIntersector.compose_FD2_OD();
                }else{ // case (b)
                    for(fd2_itr=flow_deps.begin(); fd2_itr != flow_deps.end(); ++fd2_itr) {
                        dep_t f2_dep = *fd2_itr;
                        int fd2_srcLine = f2_dep.srcLine;
                        int fd2_dstLine = f2_dep.dstLine;
                        if( fd2_srcLine == od_dstLine && fd2_dstLine == fd_dstLine ){
                            setIntersector.setFD2(f2_dep);
                            setIntersector.compose_FD2_OD();
                        }
                    }
                }
            }
            string sets_for_omega = setIntersector.subtract();
//            cout << sets_for_omega << endl;
            fstream filestr ("/tmp/oc_in.txt", fstream::out);
            filestr << sets_for_omega << endl;
            filestr.close();

            FILE *pfp = popen("/Users/adanalis/Desktop/Research/PLASMA_Distributed/Omega/omega_calc/obj/oc /tmp/oc_in.txt", "r");
            stringstream data;
            char buffer[256];
            while (!feof(pfp)){
                if (fgets(buffer, 256, pfp) != NULL){
                    data << buffer;
                }
            }
            pclose(pfp);
            string line;
            while( getline(data, line) ){
                if( line.find("#") && !line.empty() ){
//                    cout << line << endl;
                    break;
                }
            }
//            cout << endl;
            dumpDep(f_dep, line);
        }
//        cout << "-------------------------------" << endl;
    }
    // cap the last TASK
    cout << "}" << endl;

    return;
}
