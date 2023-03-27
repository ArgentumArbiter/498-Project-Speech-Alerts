In order to build the engine, follow the following instructions:

1. Download the model from the following link and place in whisper_cpp_engine:
https://drive.google.com/file/d/1DKQ2K5omtauGZtb-LIHIhYCOivum7OyI/view

2. Run $ make command

3. Edit whisper_cpp_engine/examples/command/commands.txt with any commands you want recognized

4. In whisper_cpp_engine, run $ ./command -m ./ggml-base.en.bin -cmd ./examples/command/commands.txt
