module Request = {
  module Header = {
    type t =
      | Plain(string)
      | Success(string)
      | Warning(string)
      | Error(string);

    open Json.Decode;
    open Util.Decode;

    let decode: decoder(t) =
      sum(
        fun
        | "Plain" => Contents(string |> map(text => Plain(text)))
        | "Success" => Contents(string |> map(text => Success(text)))
        | "Warning" => Contents(string |> map(text => Warning(text)))
        | "Error" => Contents(string |> map(text => Error(text)))
        | tag =>
          raise(
            DecodeError("[Request.Header] Unknown constructor: " ++ tag),
          ),
      );

    open! Json.Encode;
    let encode: encoder(t) =
      fun
      | Plain(text) =>
        object_([("tag", string("Plain")), ("contents", text |> string)])
      | Success(text) =>
        object_([("tag", string("Success")), ("contents", text |> string)])
      | Warning(text) =>
        object_([("tag", string("Warning")), ("contents", text |> string)])
      | Error(text) =>
        object_([("tag", string("Error")), ("contents", text |> string)]);
  };

  module Body = {
    type t =
      | Nothing
      | Plain(string)
      | Query(option(string), option(string));

    open Json.Decode;
    open Util.Decode;

    let decode: decoder(t) =
      sum(
        fun
        | "Nothing" => TagOnly(Nothing)
        | "Plain" => Contents(string |> map(text => Plain(text)))
        | "Query" =>
          Contents(
            pair(optional(string), optional(string))
            |> map(((placeholder, value)) => Query(placeholder, value)),
          )
        | tag =>
          raise(DecodeError("[Request.Body] Unknown constructor: " ++ tag)),
      );

    open! Json.Encode;
    let encode: encoder(t) =
      fun
      | Nothing => object_([("tag", string("Nothing"))])
      | Plain(text) =>
        object_([("tag", string("Plain")), ("contents", text |> string)])
      | Query(placeholder, value) =>
        object_([
          ("tag", string("Query")),
          (
            "contents",
            (placeholder, value)
            |> pair(nullable(string), nullable(string)),
          ),
        ]);
  };

  module InputMethod = {
    type t =
      | Activate
      | Deactivate
      | Update(string, array(string), array(string))
      | MoveUp
      | MoveRight
      | MoveDown
      | MoveLeft;

    open Json.Decode;
    open Util.Decode;

    let decode: decoder(t) =
      sum(
        fun
        | "Activate" => TagOnly(Activate)
        | "Deactivate" => TagOnly(Deactivate)
        | "Update" =>
          Contents(
            tuple3(string, array(string), array(string))
            |> map(((sequence, suggestions, candidates)) =>
                 Update(sequence, suggestions, candidates)
               ),
          )
        | "MoveUp" => TagOnly(MoveUp)
        | "MoveRight" => TagOnly(MoveRight)
        | "MoveDown" => TagOnly(MoveDown)
        | "MoveLeft" => TagOnly(MoveLeft)
        | tag =>
          raise(
            DecodeError("[Request.InputMethod] Unknown constructor: " ++ tag),
          ),
      );

    open! Json.Encode;
    let encode: encoder(t) =
      fun
      | Activate => object_([("tag", string("Activate"))])
      | Deactivate => object_([("tag", string("Deactivate"))])
      | Update(sequence, suggestions, candidates) =>
        object_([
          ("tag", string("Update")),
          (
            "contents",
            (sequence, suggestions, candidates)
            |> tuple3(string, array(string), array(string)),
          ),
        ])
      | MoveUp => object_([("tag", string("MoveUp"))])
      | MoveRight => object_([("tag", string("MoveRight"))])
      | MoveDown => object_([("tag", string("MoveDown"))])
      | MoveLeft => object_([("tag", string("MoveLeft"))]);
  };

  type t =
    | Show
    | Hide
    | InterruptQuery
    | Plain(Header.t, Body.t)
    | InputMethod(InputMethod.t);

  // JSON encode/decode

  open Json.Decode;
  open Util.Decode;

  let decode: decoder(t) =
    sum(
      fun
      | "Show" => TagOnly(Show)
      | "Hide" => TagOnly(Hide)
      | "InterruptQuery" => TagOnly(InterruptQuery)
      | "Plain" =>
        Contents(
          pair(Header.decode, Body.decode)
          |> map(((header, body)) => Plain(header, body)),
        )
      | "InputMethod" =>
        Contents(InputMethod.decode |> map(x => InputMethod(x)))
      | tag => raise(DecodeError("[Request] Unknown constructor: " ++ tag)),
    );

  open! Json.Encode;
  let encode: encoder(t) =
    fun
    | Show => object_([("tag", string("Show"))])
    | Hide => object_([("tag", string("Hide"))])
    | InterruptQuery => object_([("tag", string("InterruptQuery"))])
    | Plain(header, body) =>
      object_([
        ("tag", string("Plain")),
        ("contents", (header, body) |> pair(Header.encode, Body.encode)),
      ])
    | InputMethod(payload) =>
      object_([
        ("tag", string("InputMethod")),
        ("contents", payload |> InputMethod.encode),
      ]);
};

module Event = {
  module InputMethod = {
    type t =
      | InsertChar(string);

    open Json.Decode;
    open Util.Decode;

    let decode: decoder(t) =
      sum(
        fun
        | "InsertChar" => Contents(string |> map(char => InsertChar(char)))
        | tag =>
          raise(
            DecodeError("[Request.InputMethod] Unknown constructor: " ++ tag),
          ),
      );

    open! Json.Encode;
    let encode: encoder(t) =
      fun
      | InsertChar(char) =>
        object_([
          ("tag", string("InsertChar")),
          ("contents", char |> string),
        ]);
  };

  type t =
    | Initialized
    | Destroyed
    | InputMethod(InputMethod.t);

  open Json.Decode;
  open Util.Decode;

  let decode: decoder(t) =
    sum(
      fun
      | "Initialized" => TagOnly(Initialized)
      | "Destroyed" => TagOnly(Destroyed)
      | "InputMethod" =>
        Contents(InputMethod.decode |> map(action => InputMethod(action)))
      | tag =>
        raise(DecodeError("[Response.EVent] Unknown constructor: " ++ tag)),
    );

  open! Json.Encode;
  let encode: encoder(t) =
    fun
    | Initialized => object_([("tag", string("Initialized"))])
    | Destroyed => object_([("tag", string("Destroyed"))])
    | InputMethod(action) =>
      object_([
        ("tag", string("InputMethod")),
        ("contents", action |> InputMethod.encode),
      ]);
};

module Response = {
  type t =
    | EventPiggyBack(Event.t) // piggy-back Events from View on Response.t
    | Success
    | QuerySuccess(string)
    | QueryInterrupted;

  open Json.Decode;
  open Util.Decode;

  let decode: decoder(t) =
    sum(
      fun
      | "EventPiggyBack" =>
        Contents(Event.decode |> map(event => EventPiggyBack(event)))
      | "Success" => TagOnly(Success)
      | "QuerySuccess" =>
        Contents(string |> map(result => QuerySuccess(result)))
      | "QueryInterrupted" => TagOnly(QueryInterrupted)
      | tag => raise(DecodeError("[Response] Unknown constructor: " ++ tag)),
    );

  open! Json.Encode;
  let encode: encoder(t) =
    fun
    | Success => object_([("tag", string("Success"))])
    | QuerySuccess(result) =>
      object_([
        ("tag", string("QuerySuccess")),
        ("contents", result |> string),
      ])
    | QueryInterrupted => object_([("tag", string("QueryInterrupted"))])
    | EventPiggyBack(event) =>
      object_([
        ("tag", string("EventPiggyBack")),
        ("contents", event |> Event.encode),
      ]);
};