#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
void first_pass(const char *am_path);
void second_pass(const char *am_path);
void free_symbol_table(void);
void reset_assembler_state(void);
int  pre_assembler_main(const char *as_path);
int  get_first_pass_errors(void);
int  get_second_pass_errors(void);

/* Helper function to remove output files when errors occur */
static void remove_output_files(const char *base_name) {
    char filename[512];
    
    /* Remove .ob file */
    snprintf(filename, sizeof(filename), "%s.ob", base_name);
    remove(filename);
    
    /* Remove .ent file */
    snprintf(filename, sizeof(filename), "%s.ent", base_name);
    remove(filename);
    
    /* Remove .ext file */
    snprintf(filename, sizeof(filename), "%s.ext", base_name);
    remove(filename);
    
    printf("Output files removed due to assembly errors.\n");
}

int main(int argc, char *argv[])
{
    int i;
    int overall_success = 1;
    int total_files = 0;
    int successful_files = 0;
    char as_filename[512];
    char am_filename[512];

    if (argc < 2) {
        printf("Usage: %s <file1> <file2> ... (without .as suffix)\n", argv[0]);
        return 1;
    }

    printf("Starting assembly process...\n");

    for (i = 1; i < argc; i++) {
        int current_file_success = 1;
        char temp_file[512];
        FILE *fp;
        
        total_files++;

        /* Build .as and .am filenames from base */
        snprintf(as_filename, sizeof(as_filename), "%s.as", argv[i]);
        snprintf(am_filename, sizeof(am_filename), "%s.am", argv[i]);

        printf("\n=== Processing %s ===\n", as_filename);

        /* Reset global assembler state for new file */
        reset_assembler_state();

        /* Phase 1: Pre-assembler (macro expansion) */
        printf("Phase 1: Pre-assembler (macro expansion)...\n");
        if (pre_assembler_main(as_filename) != 0) {
            printf("ERROR: Pre-assembler failed for %s\n", as_filename);
            printf("Reason: Macro definition or usage errors\n");
            current_file_success = 0;
            overall_success = 0;
            continue; /* Skip to next file */
        }
        printf("Pre-assembler completed successfully.\n");

        /* Phase 2: First pass (symbol table and instruction encoding) */
        printf("Phase 2: First pass (symbol table and encoding)...\n");
        first_pass(am_filename);

        if (get_first_pass_errors() > 0) {
            printf("ERROR: First pass failed with %d error(s)\n", get_first_pass_errors());
            printf("Reason: Syntax errors, unknown instructions, or invalid operands\n");
            printf("Second pass will be skipped.\n");
            remove_output_files(argv[i]);
            current_file_success = 0;
            overall_success = 0;
            continue; /* Skip to next file */
        }
        printf("First pass completed successfully.\n");

        /* Phase 3: Second pass (symbol resolution and file generation) */
        printf("Phase 3: Second pass (resolution and output)...\n");
        second_pass(am_filename);

        if (get_second_pass_errors() > 0) {
            printf("ERROR: Second pass failed with %d error(s)\n", get_second_pass_errors());
            printf("Reason: Undefined symbols or output file creation errors\n");
            remove_output_files(argv[i]);
            current_file_success = 0;
            overall_success = 0;
            continue; /* Skip to next file */
        }
        printf("Second pass completed successfully.\n");

        /* If we reach here, assembly was successful */
        printf("Assembly completed successfully for %s\n", argv[i]);
        printf("Output files generated: %s.ob", argv[i]);
        
        /* Check if optional files were created */        
        snprintf(temp_file, sizeof(temp_file), "%s.ent", argv[i]);
        fp = fopen(temp_file, "r");
        if (fp) {
            printf(", %s.ent", argv[i]);
            fclose(fp);
        }
        
        snprintf(temp_file, sizeof(temp_file), "%s.ext", argv[i]);
        fp = fopen(temp_file, "r");
        if (fp) {
            printf(", %s.ext", argv[i]);
            fclose(fp);
        }
        printf("\n");

        if (current_file_success) {
            successful_files++;
        }
        printf("Cleaning up memory...\n");
        free_symbol_table();
    }

    /* Print final summary */
    printf("\n=== Assembly Summary ===\n");
    printf("Total files processed: %d\n", total_files);
    printf("Successfully assembled: %d\n", successful_files);
    printf("Failed: %d\n", total_files - successful_files);

    if (overall_success) {
        printf("Overall result: SUCCESS - All files assembled without errors\n");
    } else {
        printf("Overall result: FAILURE - Some files contained errors\n");
    }

    return overall_success ? 0 : 1;
}

