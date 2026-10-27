## To Do List

# Sensor Node 
    - [ ] Remove boot prints after each wake up
    - [ ] Discuss AI model: Will it work on warm days? normalize based on frame's temperature...? reassess model? simpler model? 
        ?Min/Max scaling: coldest frame is always 0 and warmest is 1 (some sort of quantization?) Center data around frames average temp? (Z-score standardization)
        - At least assess hard coded temperature into average of frame and retrain
    - [ ] Add sensor node numbering logic to distinguish 
