(setq project-dir default-directory)
(setq project-name "Blind")

(setq compile-command (concat default-directory "build.bat "))
(setq run-command (concat default-directory "run.bat"))

(setq frame-title-format '("" project-name ": %b")) ;; not totally sure why I need the "" before the varible.

(defun run-project ()
  (interactive)
  "Defined in project.el
runs the program that we're working on."
	(make-process :name "project-run" :buffer "*run*" :command (list run-command)))

(define-key murfy-keys-mode-map (kbd "<M-f1>") 'compile)
(define-key murfy-keys-mode-map (kbd "<f2>") 'run-project)

(find-file "./project.el" t)
(find-file "./build.bat" t)
(find-file "./run.bat" t)
(find-file "./src/*.*" t)
