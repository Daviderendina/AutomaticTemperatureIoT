class MeasureArrayHandler {

  public:
    short elementsAdded = 0;
    float* measureArray;
    int arraySize;

    float lastMean = 0;

    
  
    MeasureArrayHandler(int arraySize){
      this -> arraySize = arraySize;
      measureArray = new float[arraySize];
    }

    MeasureArrayHandler(int arraySize, float lastMean){
      this -> lastMean = lastMean;
      this -> arraySize = arraySize;
      measureArray = new float[arraySize];
    }

    float getAverage(){
      if(elementsAdded == 0)
        return lastMean;
        
      float sum = 0;
      for(int i=0; i<arraySize; i++)
        sum += measureArray[i];
      return sum / elementsAdded;
    }

    void addElement(float newValue){
      for(int i=0; i<arraySize - 1; i++){
        measureArray[i] = measureArray[i + 1];
      }
      measureArray[arraySize - 1] = newValue;

      if(elementsAdded < 6)
        elementsAdded++;
    }

    void printArray(){
      for(int i=arraySize - elementsAdded; i < arraySize; i++){
        Serial.print(measureArray[i]);
        Serial.print(F(" "));
      }
      Serial.println(F(""));
    }

    void clearMeasures(){
      lastMean = getAverage();
      for(int i = 0; i < arraySize; i++)
        measureArray[i] = 0;
      elementsAdded = 0;
    }
};
