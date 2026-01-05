import subprocess
import sys
import os
from datetime import datetime

def run_git_command(args, cwd):
    """
    Executes a git command in the specified directory.
    Returns the output if successful.
    """
    try:
        result = subprocess.run(
            args,
            cwd=cwd,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        error_msg = e.stderr.strip()
        output_msg = e.stdout.strip()
        
        if "nothing to commit" in output_msg or "nothing to commit" in error_msg:
            return "NO_CHANGES"
            
        print(f"CRITICAL ERROR executing: {' '.join(args)}")
        print(f"Git Output: {output_msg}")
        print(f"Git Error: {error_msg}")
        sys.exit(1)

def main():
    if len(sys.argv) > 1:
        project_root = sys.argv[1]
    else:
        project_root = os.path.dirname(os.path.abspath(__file__))

    print(f"Initialize Auto-Sync for: {project_root}")

    # 1. Stage Changes
    print("Staging changes...")
    run_git_command(["git", "add", "."], project_root)

    # 2. Commit Changes
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    commit_msg = f"Auto-Build Sync: {timestamp}"
    
    print(f"Committing with message: '{commit_msg}'")
    commit_status = run_git_command(["git", "commit", "-m", commit_msg], project_root)

    if commit_status == "NO_CHANGES":
        print("Status: No file changes detected. Skipping push.")
        sys.exit(0)

    # 3. Push to Remote (Force)
    # Added "--force" to overwrite remote history with local history
    print("Pushing to GitHub origin/main (FORCE)...")
    run_git_command(["git", "push", "origin", "main", "--force"], project_root)

    print("Success: Repository synchronization complete.")

if __name__ == "__main__":
    main()