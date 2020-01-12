open Jingoo;

let ensure_dir_is_empty = (d: string) =>
  if (Utils.Filename.test(Utils.Filename.Exists, d)) {
    if (Utils.Filename.test(Utils.Filename.Is_file, d)) {
      raise(
        Errors.Incorrect_destination_path("This path is not a directory."),
      );
    } else if (!List.is_empty(Utils.Sys.ls_dir(d, ~recursive=false))) {
      raise(
        Errors.Incorrect_destination_path("This directory is not empty."),
      );
    };
  };

let generate_file =
    (
      ~source_directory: string,
      ~destination_directory: string,
      ~models,
      source_file,
    ) => {
  let data =
    Stdio.In_channel.read_all(source_file) |> Jg_wrapper.from_string(~models);

  let dest =
    source_file
    |> Jg_wrapper.from_string(~models)
    |> String.substr_replace_first(
         ~pattern=source_directory,
         ~with_=destination_directory,
       );

  let parent_dir = Utils.Filename.dirname(dest);
  Utils.Filename.mkdir(parent_dir, ~parent=true);
  Utils.Filename.cp([source_file], dest);
  Stdio.Out_channel.write_all(dest, ~data);
};

let generate = (~use_defaults=false, source: Source.t, destination: string) => {
  let () = ensure_dir_is_empty(destination);

  let origin = Source.to_local_path(source);
  let template_path = Utils.Filename.concat(origin, "template");
  let template_config = Config_file.Template.parse(origin, ~use_defaults);
  let models = template_config.models;
  let doc_config = Config_file.Doc.parse(origin, ~models, ~use_defaults);

  let rec loop =
    fun
    | [] => ()
    | [f, ...fs] => {
        generate_file(
          f,
          ~source_directory=template_path,
          ~destination_directory=destination,
          ~models,
        );
        loop(fs);
      };

  Console.log(
    <Pastel>
      <Pastel> "\n🏗️  Creating a new " </Pastel>
      <Pastel color=Pastel.Blue bold=true> {doc_config.name} </Pastel>
      <Pastel> {" in " ++ destination} </Pastel>
    </Pastel>,
  );
  let template_path_regex = template_path;
  let ignore_files =
    List.map(template_config.ignore_files, ~f=file => {
      Utils.Filename.concat(template_path_regex, file)
    });
  Utils.Sys.ls_dir(template_path, ~ignore_files) |> loop;
  Console.log(
    <Pastel color=Pastel.GreenBright bold=true> "Done!\n" </Pastel>,
  );

  switch (template_config.post_installs) {
  | [] => ()
  | post_installs =>
    List.iter(
      post_installs,
      el => {
        switch (el.description) {
        | Some(description) => Console.log(<Pastel> description </Pastel>)
        | None => ()
        };

        let _ =
          Utils.Sys.exec_in_dir(
            el.command,
            ~args=el.args |> Array.of_list,
            ~dir=destination,
            ~stdout=`Dev_null,
          )
          |> Lwt_main.run;

        switch (el.description) {
        | Some(description) =>
          Console.log(
            <Pastel color=Pastel.GreenBright bold=true> "Done!\n" </Pastel>,
          )
        | None => ()
        };
      },
    )
  };

  Console.log(
    <Pastel>
      <Pastel> "🎉  Success! Created the project at " </Pastel>
      <Pastel> destination </Pastel>
    </Pastel>,
  );

  switch (doc_config.commands) {
  | [] => ()
  | commands =>
    Console.log(
      <Pastel>
        "\nHere are some example commands that you can run inside this directory:"
      </Pastel>,
    );
    List.iter(
      commands,
      ~f=el => {
        Console.log(
          <Pastel color=Pastel.Blue bold=true>
            {"\n    " ++ Jg_wrapper.from_string(el.name, ~models)}
          </Pastel>,
        );
        Console.log(
          <Pastel>
            {"      " ++ Jg_wrapper.from_string(el.description, ~models)}
          </Pastel>,
        );
      },
    );
  };

  Console.log(<Pastel> "\nHappy hacking!" </Pastel>);

  /* Remove spin configuration file from the generated project */
  Utils.Filename.rm([Utils.Filename.concat(destination, "spin")]);
  Config_file_project.save(
    {models, source: Source.to_string(source)},
    ~destination,
  );
};
