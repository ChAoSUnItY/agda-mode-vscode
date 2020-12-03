open Belt

open! Task

let handleEditorIMOutput = output => {
  open EditorIM.Output
  let handle = kind =>
    switch kind {
    | UpdateView(s, t, i) => Command.InputMethod.UpdateView(s, t, i)
    | Rewrite(xs, f) => Command.InputMethod.Rewrite(xs, f)
    | Activate => Command.InputMethod.Activate
    | Deactivate => Command.InputMethod.Deactivate
    }
  output->Array.map(handle)
}

module TempPromptIM = {
  let previous = ref("")
  let activate = (self, input) => {
    let cursorOffset = String.length(input) - 1
    previous.contents = Js.String.substring(~from=0, ~to_=cursorOffset, input)
    EditorIM.activate(self, None, [(cursorOffset, cursorOffset)])
  }
  let change = (self, input) => EditorIM.deviseChange(self, previous.contents, input)

  let handle = output => {
    open EditorIM.Output
    let handle = kind =>
      switch kind {
      | UpdateView(s, t, i) => list{DispatchCommand(InputMethod(UpdateView(s, t, i)))}
      | Rewrite(xs, f) =>
        f()
        switch xs[0] {
        | None => list{}
        | Some((_, symbol)) => list{ViewEvent(PromptIMUpdate(symbol))}
        }
      | Activate => list{DispatchCommand(InputMethod(Activate))}
      | Deactivate => list{DispatchCommand(InputMethod(Deactivate))}
      }
    output->Array.map(handle)->List.concatMany
  }
}

// from Editor Command to Tasks
let handle = x =>
  switch x {
  | Command.InputMethod.Activate => list{
      WithStateP(
        state =>
          if EditorIM.isActivated(state.editorIM) {
            // already activated, insert backslash "\" instead
            Editor.Cursor.getMany(state.editor)->Array.forEach(point =>
              Editor.Text.insert(VSCode.TextEditor.document(state.editor), point, "\\")->ignore
            )
            // deactivate
            EditorIM.deactivate(state.editorIM)
            Promise.resolved(list{ViewEvent(InputMethod(Deactivate))})
          } else {
            let document = VSCode.TextEditor.document(state.editor)
            // activated the input method with positions of cursors
            let startingRanges: array<(int, int)> =
              Editor.Selection.getMany(state.editor)->Array.map(range => (
                document->VSCode.TextDocument.offsetAt(VSCode.Range.start(range)),
                document->VSCode.TextDocument.offsetAt(VSCode.Range.end_(range)),
              ))
            EditorIM.activate(state.editorIM, Some(state.editor), startingRanges)
            Promise.resolved(list{ViewEvent(InputMethod(Activate))})
          },
      ),
    }
  | PromptChange(input) => list{
      WithStateP(
        state => {
          // activate when the user typed a backslash "/"
          let shouldActivate = Js.String.endsWith("\\", input)

          let deactivateEditorIM = () => {
            EditorIM.deactivate(state.editorIM)
            list{ViewEvent(InputMethod(Deactivate))}
          }
          let activatePromptIM = () => {
            // remove the ending backslash "\"
            let input = Js.String.substring(~from=0, ~to_=String.length(input) - 1, input)
            PromptIM.activate(state.promptIM, input)

            // update the view
            list{ViewEvent(InputMethod(Activate)), ViewEvent(PromptIMUpdate(input))}
          }

          if EditorIM.isActivated(state.editorIM) {
            if shouldActivate {
              Promise.resolved(List.concatMany([deactivateEditorIM(), activatePromptIM()]))
            } else {
              Promise.resolved(list{ViewEvent(PromptIMUpdate(input))})
            }
          } else if PromptIM.isActivated(state.promptIM) {
            PromptIM.update2(state.promptIM, input)->TempPromptIM.handle->Promise.resolved
          } else if shouldActivate {
            Promise.resolved(activatePromptIM())
          } else {
            Promise.resolved(list{ViewEvent(PromptIMUpdate(input))})
          }
        },
      ),
    }
  | Rewrite(replacements, resolve) => list{
      WithStateP(
        state => {
          let document = state.editor->VSCode.TextEditor.document
          let replacements = replacements->Array.map(((interval, text)) => {
            let range = VSCode.Range.make(
              document->VSCode.TextDocument.positionAt(fst(interval)),
              document->VSCode.TextDocument.positionAt(snd(interval)),
            )
            (range, text)
          })
          Editor.Text.batchReplace(document, replacements)->Promise.map(_ => {
            resolve()
            list{}
          })
        },
      ),
    }
  | Deactivate => list{
      WithState(
        state => {
          EditorIM.deactivate(state.editorIM)
          PromptIM.deactivate(state.promptIM)
        },
      ),
      ViewEvent(InputMethod(Deactivate)),
    }

  | UpdateView(sequence, translation, index) => list{
      ViewEvent(InputMethod(Update(sequence, translation, index))),
    }
  | InsertChar(char) => list{
      WithStateP(
        state =>
          if EditorIM.isActivated(state.editorIM) {
            let char = Js.String.charAt(0, char)
            Editor.Cursor.getMany(state.editor)->Array.forEach(point =>
              Editor.Text.insert(VSCode.TextEditor.document(state.editor), point, char)->ignore
            )
            Promise.resolved(list{})
          } else if PromptIM.isActivated(state.promptIM) {
            PromptIM.insertChar2(state.promptIM, char)->TempPromptIM.handle->Promise.resolved
          } else {
            Promise.resolved(list{})
          },
      ),
    }
  | ChooseSymbol(symbol) => list{
      WithStateP(
        state =>
          if EditorIM.isActivated(state.editorIM) {
            EditorIM.run(state.editorIM, Some(state.editor), Candidate(ChooseSymbol(symbol)))
            ->Promise.map(handleEditorIMOutput)
            ->Promise.map(xs => xs->List.fromArray->List.map(x => DispatchCommand(InputMethod(x))))
          } else if PromptIM.isActivated(state.promptIM) {
            PromptIM.chooseSymbol(state.promptIM, symbol)->TempPromptIM.handle->Promise.resolved
          } else {
            Promise.resolved(list{})
          },
      ),
    }
  | MoveUp => list{
      WithStateP(
        state =>
          EditorIM.run(state.editorIM, Some(state.editor), Candidate(BrowseUp))
          ->Promise.map(handleEditorIMOutput)
          ->Promise.map(xs => xs->List.fromArray->List.map(x => DispatchCommand(InputMethod(x)))),
      ),
    }
  | MoveRight => list{
      WithStateP(
        state =>
          EditorIM.run(state.editorIM, Some(state.editor), Candidate(BrowseRight))
          ->Promise.map(handleEditorIMOutput)
          ->Promise.map(xs => xs->List.fromArray->List.map(x => DispatchCommand(InputMethod(x)))),
      ),
    }
  | MoveDown => list{
      WithStateP(
        state =>
          EditorIM.run(state.editorIM, Some(state.editor), Candidate(BrowseDown))
          ->Promise.map(handleEditorIMOutput)
          ->Promise.map(xs => xs->List.fromArray->List.map(x => DispatchCommand(InputMethod(x)))),
      ),
    }
  | MoveLeft => list{
      WithStateP(
        state =>
          EditorIM.run(state.editorIM, Some(state.editor), Candidate(BrowseLeft))
          ->Promise.map(handleEditorIMOutput)
          ->Promise.map(xs => xs->List.fromArray->List.map(x => DispatchCommand(InputMethod(x)))),
      ),
    }
  }
