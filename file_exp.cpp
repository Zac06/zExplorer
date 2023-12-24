#include<iostream>
#include<filesystem>
#include<string>
#include<vector>
#include<algorithm>
#include<deque>

#include<ctime>
#include<cstring>

#include<unistd.h>
#include<signal.h>
#include<sys/stat.h>

#ifdef _WIN32
    #include<windows.h>
#endif

#include<ncurses.h>


using namespace std;

//Layout properties

#define cwd_x 1
#define cwd_y 1
#define cwd_w (scr_width-24)
#define cwd_h 3

#define entry_list_x 1
#define entry_list_y (cwd_h+cwd_y)
#define entry_list_w (scr_width-24)
#define entry_list_h (scr_height-4-cwd_h-cwd_y)

#define file_input_x 1
#define file_input_y (entry_list_h+entry_list_y)
#define file_input_w entry_list_w
#define file_input_h /*(scr_height-entry_list_h)*/ 3

#define guide_x (entry_list_x+entry_list_w+1)
#define guide_y 1
#define guide_w 20
#define guide_h 13

#define properties_x guide_x
#define properties_y (guide_y+guide_h)
#define properties_w guide_w
#define properties_h (scr_height-properties_y-1)

// additional keys for controls handling

#define KEY_CTRL_O 15
#define KEY_CTRL_W 23
#define KEY_CTRL_R 18
#define KEY_CTRL_U 21
#define KEY_CTRL_L 12
#define KEY_NEWLINE 10
#define KEY_CTRL_P 16
#define KEY_CTRL_K 11
#define KEY_CTRL_G 7
#define KEY_CTRL_T 20
#define KEY_CTRL_B 2

#ifdef _WIN32
    #define open_string "start \"\" \""+entry_name+"\""
    #define path_sep '/'
    #define path_sep_str "/"
    #define root_str_len 3
#else
    #define open_string "open \""+entry_name+"\""+"> /dev/null 2>&1"
    #define st_mtime st_mtim.tv_sec
    #define path_sep '/'
    #define path_sep_str "/"
    #define root_str_len 1
#endif


/// @brief Checks if a path is the root or not
/// @param path std::string to check
/// @return true if the given path is the root directory, false if not
bool is_root_dir(string &path){
    #ifdef _WIN32
        return  path[path.size()-2]==':'
                ||
                path[path.size()-1]==path_sep;
    #else
        return  path.size()==0
                ||
                path[path.size()-1]=='/';
    #endif
}

/// @brief Gets the filename given a pathname
/// @param p Pathname of the entry
/// @return String containing the filename (with extension)
string get_name_from_path(filesystem::path p){
    string tmp=p.generic_string();
    for(int i=tmp.size()-1; i>=0; i--){     //iterate backwards searching for the first directory separator
        if(tmp[i]==path_sep){
            return tmp.substr(i+1, tmp.size()-i-1);
            break;
        }
    }
    return tmp;
}

/// @brief Used for sorting. Confronts 2 strings ignoring case.
/// @param first String no. 1 to compare.
/// @param second String no. 2 to compare.
/// @return true if the first string is "less" than the seconds, false otherwise
bool std_string_casecmp(string& first, string& second){
    return strcasecmp(first.c_str(), second.c_str())<0;
}

/// @brief Prints a string to a given window.
/// @param win Window to print the string on
/// @param str String to print
void print_str(WINDOW* win, string& str){
    mvwprintw(win, 1,1,"%s",string(getmaxx(win)-2, ' ').c_str());
    mvwprintw(win, 1,1,"%s",str.substr(0, getmaxx(win)-2).c_str());
}

/// @brief Prints an error window.
/// @param win Main window to print a new one on (extracts the coordinates to print it centered)
/// @param err_msg String containing the error to print
void print_err_win(WINDOW* win, string err_msg){
    int x,y;
    getmaxyx(win, y,x);     //gets the win size
    WINDOW* err_win=newwin(3, err_msg.size()+2, y/2-1, x/2-err_msg.size()/2-1);     //creates a new window centered to the given one
    box(err_win, 0, 0);                                                             //creates a frame for the window
    //mvwprintw(err_win, 0,0,"%s", "Errore");
    print_str(err_win, err_msg);
    wrefresh(err_win);
    //getch();
    sleep(2);
    wclear(err_win);
    wrefresh(err_win);
    delwin(err_win);
}

/// @brief Gets the entries in the current working directory
/// @param cwd Current working directory to extract the entries from
/// @param entry_list Vector of string passed by reference that updates with the new entry list.
/// @param main_win Window to eventually print an error on.
void get_entry_list(filesystem::path cwd, vector<string> &entry_list, WINDOW* main_win){
    error_code dir_fetch_err;
    error_condition err_cond;
    entry_list=vector<string>();        //empties the vector
    for(filesystem::directory_iterator it(cwd, dir_fetch_err); it!=filesystem::directory_iterator(); it.increment(dir_fetch_err)){      //iterates over the entries
        if(it->is_directory()){
            entry_list.push_back("!    "+get_name_from_path(it->path()));       //if directory, use a ! as marker
        }else{
            entry_list.push_back("#    "+get_name_from_path(it->path()));       //if file, use a # as marker
        }
    }
    sort(entry_list.begin(), entry_list.end(), std_string_casecmp);             //sorts in alphabetical order, ignoring case

}

/// @brief Subfunction used by print_vector to print a single line to a window
/// @param win Window to print on
/// @param row_cursor Row in the window to start printing from. Incremented by 1 if print is successful
/// @param col_cursor Column in the window to start printing from
/// @param str String to print
/// @param blank_str Blank string to print to clear the row
/// @return -1 if print failed because row_cursor exceeds the window box, 0 otherwise
int print_to_window(WINDOW *win, int &row_cursor, int &col_cursor, string &str, string &blank_str){
    int w, h;
    
    getmaxyx(win, h, w);        //gets size
    if(row_cursor>=h-1){        //if the row given does not fit into the window frame, exit
        return -1;
    }
    mvwprintw(win, row_cursor, col_cursor, "%s", blank_str.c_str());                    //print a blank substring that fits into the width of the window
    mvwprintw(win, row_cursor, col_cursor, "%s", str.substr(0, w-2).c_str());           //print a substring that fits into the width of the window
    row_cursor++;
    return 0;
}

/// @brief Generic function to print a vector of strings to a window.
/// @param win Window to print on.
/// @param v_str Vector of strings. Passed by reference, though does not get modified.
/// @param start Index of v_str to start printing from
/// @param selected Index of v_str to highlight during print.
void print_vector(WINDOW *win, vector<string> &v_str, int start, int selected){
    int ret=0;      //stores return value of print_to_window
    int row_cursor=1, col_cursor=1;     //starts to print from the top left
    string blank_str(getmaxx(win)-2, ' ');      //creates a blank string, wide as the window, to clean the rows.
    for(int i=start; i<v_str.size()&&ret!=-1; i++){     //iterates over strings, starting from start
        if(i==selected){                                //if the current element index equals to the selected index, highlight.
            wattr_on(win, A_REVERSE, 0);
        }
        ret=print_to_window(win, row_cursor, col_cursor, v_str[i], blank_str);      //print the string with whatever attributes are set.
        if(i==selected){
            wattr_off(win, A_REVERSE, 0);
        }
    }
    while(ret!=-1){
        ret=print_to_window(win, row_cursor, col_cursor, blank_str, blank_str);     //print the rest of the window with blank strings (to empty it)
    }
}

/// @brief Manipulates the current working directory to go to the parent directory. Checks for root.
/// @param cwd Path to manipulate
/// @return String containing the new path.
string go_back_one_dir(filesystem::path cwd){
    string tmp=cwd.generic_string();
    
    int size_to_remove=get_name_from_path(tmp).size();
    tmp.erase(tmp.size()-size_to_remove, size_to_remove);
    if(tmp.size()>root_str_len){
        tmp.pop_back();
    }
    return tmp;
}

/// @brief Deletes all the windows (and sets them to NULL)
/// @param main_win Pointer to the main window pointer
/// @param entry_list_win Pointer to the entry_list window pointer
/// @param file_input_win Pointer to the search input window pointer
/// @param guide_win Pointer to the guide window pointer
/// @param properties_win Pointer to the properties window pointer
/// @param cwd_win Pointer to the current working directory window pointer. 
void del_windows(WINDOW** main_win, WINDOW** entry_list_win, WINDOW** file_input_win, WINDOW** guide_win, WINDOW** properties_win, WINDOW** cwd_win){
    if(!*entry_list_win){           //clear, delete and set to NULL every window EXCEPT the main window
        wclear(*entry_list_win);
        delwin(*entry_list_win);
        *entry_list_win=NULL;
    }
    if(!*file_input_win){
        wclear(*file_input_win);
        delwin(*file_input_win);
        *file_input_win=NULL;
    }
    if(!*guide_win){
        wclear(*guide_win);
        delwin(*guide_win);
        *guide_win=NULL;
    }
    if(!*properties_win){
        wclear(*properties_win);
        delwin(*properties_win);
        *properties_win=NULL;
    }
    if(!*cwd_win){
        wclear(*cwd_win);
        delwin(*cwd_win);
        *cwd_win=NULL;
    }
    
}

/// @brief Deletes and recreates the given windows to fit a new size. If windows are NULL, only creates them.
/// @param main_win Pointer to the main window pointer
/// @param entry_list_win Pointer to the entry_list window pointer
/// @param file_input_win Pointer to the search input window pointer
/// @param guide_win Pointer to the guide window pointer
/// @param properties_win Pointer to the properties window pointer
/// @param cwd_win Pointer to the current working directory window pointer. 
void refresh_window_sizes(WINDOW** main_win, WINDOW** entry_list_win, WINDOW** file_input_win, WINDOW** guide_win, WINDOW** properties_win, WINDOW** cwd_win){
    //deletes the windows (if they are NULL set them up)
    del_windows(main_win, entry_list_win, file_input_win, guide_win, properties_win, cwd_win);
    
    clear();        //refresh the main window
    refresh();
    endwin();
    refresh();
    initscr();
    
    int scr_height, scr_width;                      //declaration needed for the defines 
    getmaxyx(*main_win, scr_height, scr_width);     //gets the main window size

    *entry_list_win=newwin(entry_list_h,entry_list_w,entry_list_y,entry_list_x);        //create new windows
    *file_input_win=newwin(file_input_h, file_input_w, file_input_y, file_input_x);
    *guide_win=newwin(guide_h, guide_w, guide_y, guide_x);
    *properties_win=newwin(properties_h, properties_w, properties_y, properties_x);
    *cwd_win=newwin(cwd_h, cwd_w, cwd_y, cwd_x);

    box(*entry_list_win, 0,0);      //create new window frames
    box(*file_input_win, 0,0);
    box(*guide_win, 0,0);
    box(*properties_win, 0,0);
    box(*cwd_win, 0,0);

    mvwprintw(*entry_list_win, 0,0, "Lista elementi");      //print an identifier to every window
    mvwprintw(*file_input_win, 0,0, "Nome/ricerca");
    mvwprintw(*guide_win, 0,0, "Guida");
    mvwprintw(*properties_win, 0,0, "Proprieta\'");
    mvwprintw(*cwd_win, 0,0, "Percorso");

    
}

/// @brief Refreshes the windows (for example, after printing)
/// @param main_win Pointer to the main window pointer
/// @param entry_list_win Pointer to the entry_list window pointer
/// @param file_input_win Pointer to the search input window pointer
/// @param guide_win Pointer to the guide window pointer
/// @param properties_win Pointer to the properties window pointer
/// @param cwd_win Pointer to the current working directory window pointer. 
void refresh_windows(WINDOW** main_win, WINDOW** entry_list_win, WINDOW** file_input_win, WINDOW** guide_win, WINDOW** properties_win, WINDOW** cwd_win){
    wrefresh(*main_win);            //refresh the various windows
    wrefresh(*entry_list_win);
    wrefresh(*file_input_win);
    wrefresh(*guide_win);
    wrefresh(*properties_win);
    wrefresh(*cwd_win);
}

/// @brief Moves the main cursor to a certain position in a certain window
/// @param win 
/// @param col_cursor 
void move_to_cursor(WINDOW* win, int col_cursor){
    move(getbegy(win)+1,getbegx(win)+1+col_cursor);     //moves the MAIN CURSOR
}

/// @brief Performs a case-sensitive search over the entries given, starting from a certain index (used for multiple correspondences)
/// @param name Name of the entry to search for
/// @param entries Vector of entries (passed by reference, does not get modified)
/// @param start Starting index to search from
/// @return -1 if not found, index of correspondence if found
int search_entry(string &name, vector<string> &entries, int start){
    for(int i=start; i<entries.size(); i++){    //iteration
        if((int)entries[i].find(name, 5)!=-1){  //search starts from index 5 for the entries since they start with a type indicator 5 characters long
            return i;
        }
    }
    
    return -1;  //not found
}

/// @brief Gets the properties via a string vector passed by reference, that has to respect some formatting.
/// @param name Name of the file
/// @param cwd Current working directory
/// @param prop Vector of strings propertly formatted to contain the properties
/// @param main_win Main window to eventually print any errors (doesn't still, idk how to manage them)
void get_file_properties(string &name, string& cwd, vector<string> &prop, WINDOW* main_win){
    string entry_name=cwd+path_sep_str+name.substr(5, name.size());     //pathname of entry to fetch
    error_code err_code;
    error_condition err_cond;

    filesystem::directory_entry tmp_entry(entry_name, err_code);        //creates entry to fetch
    err_cond=err_code.default_error_condition();

    prop[1]=tmp_entry.is_directory() ? "Cartella" : "File";             //identifies type

    struct stat tmp_struct;                                             //gets the last edit time
    stat(entry_name.c_str(), &tmp_struct);
    prop[4]=ctime(&tmp_struct.st_mtime);                                //transforms into string

    int i, spacecount;
    for(i=0, spacecount=0; i<prop[4].size()&&spacecount<3; i++){        //searches for 3 spaces; when the 3rd is found, splits the string into 2 pieces: time and date, to be printed in 2 different lines
        if(prop[4][i]==' '){
            spacecount++;
        }
    }
    prop[5]=prop[4].substr(i,prop[4].size());                           //separates the 2 parts of last edit time
    prop[4].erase(i, prop[4].size());
    prop[5].pop_back(); //remove \n

    if(tmp_entry.is_directory()){                                       //determines size: no size of a folder
        prop[8]="-";
    }else{                                                              //if a file, fetches the size, and transforms it into a more human-readable format (B, KB, MB, ...)
        long double fsize=(long double)tmp_entry.file_size(err_code);
        err_cond=err_code.default_error_condition();

        if(fsize==-1){
            prop[8]="-";    //unavailable
        }

        int i=0;
        const string names[5]={"B", "KB", "MB", "GB", "TB"};
        while(i<4&&(int)(fsize/1000)>0){
            i++;
            fsize/=1000;
        }
        prop[8]=to_string(fsize).substr(0, to_string(fsize).find('.')+3)+" "+names[i];
    }
}

int main(int argc, char** argv){        //i know the main is REALLY long
    string cwd=filesystem::current_path().generic_string();     //contains the actual current directory

    //Declaration of the various windows
    WINDOW* main_win=initscr(), *entry_list_win=NULL, *file_input_win=NULL, *guide_win=NULL, *properties_win=NULL, *cwd_win=NULL;
    
    if(!has_colors()){  //if the terminal does not have support, terminate
        endwin();
        cout<<"\nYOUR TERMINAL DOES NOT SUPPORT COLORS";
        return -1;
    }

    start_color();      //start color support
    use_default_colors();       //use default colors for the execution of the program

    //start the windows; all of them are already null so it acts like a starter
    refresh_window_sizes(&main_win, &entry_list_win, &file_input_win, &guide_win, &properties_win, &cwd_win);
    
    vector<string> entry_list;                      //vector of strings to contain the entry names
    vector<string> guide({  "[Ctrl+O] Apri",        //vector of strings containing the guide
                            "[Ctrl+R] Aggiorna", 
                            "[Ctrl+U] Car. sup.", 
                            "[su/giu] Scorri", 
                            "[Ctrl+L] Focus", 
                            "[Ctrl+P] De/selez.",
                            "[Ctrl+K] Sel.tutto",
                            "[Ctrl+G] Des.tutto",
                            "[Ctrl+T] Copia sel",
                            "[Ctrl+B] Incolla",
                            "[Ctrl+W] Elim. sel"});
                            
    vector<string> entry_properties={   "Tipo:", "", "",                //template to print the properties using the same functions as the entries and guide
                                        "Data modifica:", "","","",
                                        "Dimensione:", "", ""};

    deque<string> copy_queue;           //queue to copy the files/folders

    string search_input("");            //contains the input in the search section
    string cwd_input(cwd);              //contains the input in the current directory section

    //various cursors
    int entry_list_cursor=0, entry_list_start=0;
    bool program_loop=true, box_highlight=true;
    int file_input_cursor=0, cwd_cursor=cwd_input.size();
    int search_mode=-1;

    keypad(main_win, true);                         //sets to get also non-letter input (i.e. arrows)
    noecho();                                       //typed keys do not get displayed
    get_entry_list(cwd, entry_list, main_win);      //gets the entries in the current cwd
    print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);      //prints the entries
    print_str(cwd_win, cwd);                        //prints the current directory path
    print_vector(guide_win, guide, 0, -1);          //prints the guide
    print_str(file_input_win, search_input);        //prints the search input
    if(entry_list.size()){                          //gets the file properties (only if there are entries available)
        get_file_properties(entry_list[entry_list_cursor], cwd, entry_properties, main_win);
    }
    print_vector(properties_win, entry_properties, 0,-1);       //prints the properties
    
    while(program_loop){                            //while true
        //refresh windows and move cursor to the "right" location (the search or the cwd box)
        refresh_windows(&main_win, &entry_list_win, &file_input_win, &guide_win, &properties_win, &cwd_win);
        move_to_cursor(box_highlight ? file_input_win : cwd_win, box_highlight ? file_input_cursor : cwd_cursor);

        int ch=getch();     //gets character or key
        switch(ch){
            case KEY_RESIZE:{       //terminal resized (!!DOES NOT WORK ON WIN32!!)
                //refresh windows, print everything again
                refresh_window_sizes(&main_win, &entry_list_win, &file_input_win, &guide_win, &properties_win, &cwd_win);
                print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                print_str(cwd_win, cwd);
                print_vector(guide_win, guide, 0, -1);
                print_str(file_input_win, search_input);
                print_vector(properties_win, entry_properties, 0,-1);
                break;
            }

            case KEY_UP:{           //moves current cursor up; if exceeds the windows limits, also move the start
                if(entry_list_cursor){
                    entry_list_cursor--;
                    if(entry_list_start>entry_list_cursor){
                        entry_list_start--;
                    }
                }
                //print the new list again, re-elaborate the properties
                print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                if(entry_list.size()){
                    get_file_properties(entry_list[entry_list_cursor], cwd, entry_properties, main_win);
                }
                print_vector(properties_win, entry_properties, 0,-1);
                break;
            }

            case KEY_DOWN:{     //moves current cursor down; if exceeds the windows limits, also move the start
                if(entry_list_cursor<entry_list.size()-1){
                    entry_list_cursor++;
                    if(getmaxy(entry_list_win)-2<=entry_list_cursor-entry_list_start){
                        entry_list_start++;
                    }
                }
                //print the new list again, re-elaborate the properties
                print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                if(entry_list.size()){
                    get_file_properties(entry_list[entry_list_cursor], cwd, entry_properties, main_win);
                }
                print_vector(properties_win, entry_properties, 0,-1);
                break;
            }

            case KEY_CTRL_O:{   //opens the directory and changes the cwd if a directory, else try to open the file with the default program
                string entry_name=cwd+(!is_root_dir(cwd) ? path_sep_str : "")+entry_list[entry_list_cursor].substr(5, entry_list[entry_list_cursor].size());
                
                filesystem::directory_entry temp_entry(entry_name);     //create the entry to fetch if is a directory or not

                if(temp_entry.is_directory()){
                    cwd=entry_name;
                    get_entry_list(cwd, entry_list, main_win);
                    entry_list_start=0;
                    entry_list_cursor=0;

                    print_vector(entry_list_win, entry_list, entry_list_start,entry_list_cursor);
                    print_str(cwd_win, cwd);
                    cwd_input=cwd;
                    cwd_cursor=cwd_input.size();
                }else{
                    if(system((open_string).c_str())){  //try opening the file; if error is caused, display an error
                        //refresh everything (i don't have the necessary braincells to write an efficient function)
                        refresh_window_sizes(&main_win, &entry_list_win, &file_input_win, &guide_win, &properties_win, &cwd_win);
                        print_err_win(main_win, "Errore apertura file/nessun programma prefedinito");
                        print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                        print_str(cwd_win, cwd);
                        print_vector(guide_win, guide, 0, -1);
                        print_str(file_input_win, search_input);
                        print_vector(properties_win, entry_properties, 0,-1);
                    }
                }
                break;
            }

            case KEY_CTRL_U:{           //go up to the previous directory; re-elaborates the entries, sets cursors to 0, changes the cwd
                cwd=go_back_one_dir(cwd);
                get_entry_list(cwd, entry_list, main_win);
                entry_list_start=0;
                entry_list_cursor=0;

                print_vector(entry_list_win, entry_list, entry_list_start,entry_list_cursor);
                print_str(cwd_win, cwd);
                cwd_input=cwd;
                cwd_cursor=cwd_input.size();
                if(entry_list.size()){
                    get_file_properties(entry_list[entry_list_cursor], cwd, entry_properties, main_win);
                }
                print_vector(properties_win, entry_properties, 0,-1);
                break;
            }

            case KEY_CTRL_R:{       //re-elaborates the entries
                get_entry_list(cwd, entry_list, main_win);
                entry_list_start=0;
                entry_list_cursor=0;

                print_vector(entry_list_win, entry_list, entry_list_start,entry_list_cursor);
                if(entry_list.size()){
                    get_file_properties(entry_list[entry_list_cursor], cwd, entry_properties, main_win);
                }
                print_vector(properties_win, entry_properties, 0,-1);
                break;
            }

            case KEY_CTRL_L:{       //changes from search input to cwd and viceversa
                box_highlight=!box_highlight;
                break;
            }
            
            case KEY_CTRL_P:{       //select the "cursored" element
                if(entry_list.size()){
                    if(entry_list[entry_list_cursor][2]=='S'){
                        entry_list[entry_list_cursor][2]=' ';
                    }else{
                        entry_list[entry_list_cursor][2]='S';
                    }
                }
                print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                break;
            }

            case KEY_CTRL_K:{       //selects all elements
                for(int i=0; i<entry_list.size(); i++){
                    entry_list[i][2]='S';
                }
                print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                break;
            }

            case KEY_CTRL_G:{       //deselects all elements
                for(int i=0; i<entry_list.size(); i++){
                    entry_list[i][2]=' ';
                }
                print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                break;
            }

            case KEY_CTRL_T:{       //pushes the selected elements to the copy queue
                copy_queue.clear();
                for(int i=0; i<entry_list.size(); i++){
                    if(entry_list[i][2]=='S'){
                        copy_queue.push_back(cwd+(cwd!="/" ? "/" : "")+entry_list[i].substr(5, entry_list[i].size()));
                    }
                }
                break;
            }

            case KEY_CTRL_B:{       //pops the elements in the copy queue one by one and pastes the files to the new cwd
                string temp=cwd+path_sep_str;
                error_code err_code;
                while(!copy_queue.empty()){     //empty the queue one by one
                    if(copy_queue.front()==temp+get_name_from_path(copy_queue.front())){        //if source and destination are the same, halt the operation
                        print_err_win(main_win, "Sorgente e destinaz. coincidono. Operaz. interrotta");
                        break;
                    }
                    //copy the files (I DON'T KNOW HOW TO MANAGE ERROR CODES. I KNOW IT'S A LITERAL HERESY)
                    filesystem::copy(copy_queue.front(), temp+get_name_from_path(copy_queue.front()), filesystem::copy_options::overwrite_existing | filesystem::copy_options::recursive, err_code);
                    copy_queue.pop_front();
                }
                copy_queue.clear();
                get_entry_list(cwd, entry_list, main_win);      //refresh and print the new elements
                print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                break;
            }

            case KEY_CTRL_W:{                   //deletes the selected elements
                error_code err_code;
                for(int i=0; i<entry_list.size(); i++){
                    if(entry_list[i][2]=='S'){
                        filesystem::remove(cwd+(cwd!="/" ? "/" : "")+entry_list[i].substr(5, entry_list[i].size()), err_code);
                        entry_list_start=0;
                        entry_list_cursor=0;
                    }
                }
                get_entry_list(cwd, entry_list, main_win);      //refreshes
                print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                break;
            }

            case KEY_NEWLINE:{                  //used for:
                if(box_highlight){              //searching the various elements (keeping pressing ENTER iterates over all the found elements)
                    search_mode=search_entry(search_input, entry_list, search_mode+1);
                    if(search_mode!=-1){
                        entry_list_cursor=search_mode;
                        entry_list_start=search_mode;
                        print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                    }else{
                        entry_list_start=0;
                        entry_list_cursor=0;
                        print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                    }
                }else{                          //otherwise searchs for the cwd directory inserted. If does not exist or is a file, the cwd goes back to previous
                    filesystem::directory_entry temp_entry(cwd_input);
                    if(temp_entry.exists()&&temp_entry.is_directory()){
                        cwd=cwd_input;
                        get_entry_list(cwd, entry_list, main_win);
                        entry_list_start=0;
                        entry_list_cursor=0;
                        print_vector(entry_list_win, entry_list, entry_list_start, entry_list_cursor);
                        print_str(cwd_win, cwd);
                        cwd_cursor=cwd_input.size();
                    }else{
                        cwd_input=cwd;
                        print_str(cwd_win, cwd);
                        cwd_cursor=cwd_input.size();
                    }
                }
                if(entry_list.size()){      //print properties
                    get_file_properties(entry_list[entry_list_cursor], cwd, entry_properties, main_win);
                }
                print_vector(properties_win, entry_properties, 0,-1);
                break;
            }

            case KEY_BACKSPACE:{            //deletes the character where the cursor is positioned, for the cwd_input or the search_input
                if(box_highlight&&file_input_cursor>0){
                    file_input_cursor--;
                    if(search_input.size()){
                        search_input.erase(file_input_cursor, 1);
                    }
                    print_str(file_input_win, search_input);
                }else if(cwd_cursor>0&&!box_highlight){
                    cwd_cursor--;
                    if(cwd_input.size()){
                        cwd_input.erase(cwd_cursor, 1);
                    }
                    print_str(cwd_win, cwd_input);
                }
                break;
            }

            case KEY_LEFT:{                 //goes to the leftmost character to the one where the cursor is positioned, for the cwd_input or the search_input
                if(box_highlight&&file_input_cursor>0){
                    file_input_cursor--;
                }else if(cwd_cursor>0&&!box_highlight){
                    cwd_cursor--;
                }
                break;
            }

            case KEY_RIGHT:{                 //goes to the leftmost character to the one where the cursor is positioned, for the cwd_input or the search_input
                if(box_highlight&&file_input_cursor<search_input.size()){
                    file_input_cursor++;
                }else if(cwd_cursor<cwd_input.size()&&!box_highlight){
                    cwd_cursor++;
                }
                break;
            }

            default:{                        //anything else is managed here
                if(ch>=32){     //if a printable character
                    if(box_highlight){      //insert it into the search string and manage cursors; print it back
                        int tx, ty;
                        string tmp_str;
                        getyx(main_win, ty, tx);
                        int tmp=tx-(getbegx(file_input_win)+1);

                        tmp_str.push_back((char)ch);
                        search_input.insert(tmp, tmp_str);      //insert needs a string (i think?)
                        print_str(file_input_win, search_input);
                        file_input_cursor++;
                    }else{                  //insert it into the cwd string and manage cursors; print it back
                        int tx, ty;
                        string tmp_str;
                        getyx(main_win, ty, tx);
                        int tmp=tx-(getbegx(cwd_win)+1);

                        tmp_str.push_back((char)ch);
                        cwd_input.insert(tmp, tmp_str);         //insert needs a string (i think?)
                        print_str(cwd_win, cwd_input);
                        cwd_cursor++;
                    }
                }
                break;
            }
        }

    }

    

    endwin();
    return 0;
}

/// @author Francescato Zaccaria
/// @brief I'm still a student. This work was made out of pure challenge to myself (also to make something easier to understand to my brother than the
/// terminal commands since his computer couldn't even load the Downloads folder.).
/// @bug Many of the errors are not managed. This is because I haven't fully understood how error_code(s) work and neither error_condition(s). Appreciatable if someone fixed them,
/// possibly using the print_err_win function.