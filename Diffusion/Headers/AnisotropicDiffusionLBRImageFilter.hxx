//
//  AnisotropicDiffusionLBRImageFilter.hxx
//  itkDiffusion
//
//  Created by Jean-Marie Mirebeau on 28/02/2014.
//
//

#ifndef itkDiffusion_AnisotropicDiffusionLBRImageFilter_hxx
#define itkDiffusion_AnisotropicDiffusionLBRImageFilter_hxx

namespace itk {

    template<typename TI, typename TS>
    void AnisotropicDiffusionLBRImageFilter<TI, TS>::GenerateData(){
        typename ImageType::Pointer inputImage=const_cast<ImageType*>(this->GetInput());
        typename ImageType::Pointer image = inputImage;
        
        typedef typename ImageType::SpacingType SpacingType;
        const SpacingType referenceSpacing = inputImage->GetSpacing();

        //        const SpacingType unitSpacing(1); // Better below for non-uniform spacing.
        double minSpacing = referenceSpacing[0];
        for(int i=1; i<(int)Dimension; ++i) minSpacing = std::min(minSpacing,referenceSpacing[i]);
        const SpacingType unitSpacing = referenceSpacing/minSpacing;
        
        if(m_Adimensionize) inputImage->SetSpacing(unitSpacing);
        
        ScalarType remainingTime = m_DiffusionTime;
        
        while(remainingTime>0){
            ComputeDiffusionTensors(image);
            typename LinearDiffusionFilterType::Pointer linearDiffusionFilter = LinearDiffusionFilterType::New();
            linearDiffusionFilter->SetMaxNumberOfTimeSteps(m_MaxTimeStepsBetweenTensorUpdates);
            linearDiffusionFilter->SetRatioToMaxStableTimeStep(m_RatioToMaxStableTimeStep);
            
            linearDiffusionFilter->SetInputImage(image);
            linearDiffusionFilter->SetInputTensor(tensorImage);
            linearDiffusionFilter->SetMaxDiffusionTime(remainingTime);
            linearDiffusionFilter->Update();
            image = linearDiffusionFilter->GetOutput();
            remainingTime -= linearDiffusionFilter->GetEffectiveDiffusionTime();

            m_LinearFilterEffectiveTimesAndIterations.push_back(std::pair<ScalarType,int>(linearDiffusionFilter->GetEffectiveDiffusionTime(),linearDiffusionFilter->GetEffectiveNumberOfTimeSteps()));
            
            this->UpdateProgress(1.-remainingTime/m_DiffusionTime);
        }
        
        if(m_Adimensionize) {
            inputImage->SetSpacing(referenceSpacing);
            image->SetSpacing(referenceSpacing);
        }
        this->GraftOutput(image);
    }
    
    
    
    template<typename TI, typename TS>
    struct AnisotropicDiffusionLBRImageFilter<TI,TS>::DiffusionTensorFunctor {
        Self * eigenValuesFunctor;
        struct OrderingType;
        TensorType operator()(const TensorType & S){
            EigenValuesArrayType eigenValues;
            typename TensorType::EigenVectorsMatrixType eigenVectors;
            S.ComputeEigenAnalysis(eigenValues,eigenVectors);
            
            // For convenience, eigenvalues are sorted by increasing order
            Vector<int,Dimension> order;
            for(int i=0; i<(int)Dimension; ++i) order[i]=i;
            
            OrderingType ordering(eigenValues);
            
            std::sort(order.Begin(), order.End(),ordering);
            
            std::sort(eigenValues.Begin(), eigenValues.End());
            EigenValuesArrayType ev = this->eigenValuesFunctor->EigenValuesTransform(eigenValues);
            
            TensorType DiffusionTensor;
            for(int i=0; i<(int)Dimension; ++i){
                DiffusionTensor(order[i],order[i]) = ev[i];
                for(int j=0; j<i; ++j) DiffusionTensor(i,j) = 0.;
            }
            return DiffusionTensor.Rotate(eigenVectors.GetTranspose());
        }
    };
    
     // c++ 11 would be : [& eigenValues](int i, int j)->bool {return eigenValues[i]<eigenValues[j];}
    template<typename TI, typename TS>
    struct AnisotropicDiffusionLBRImageFilter<TI,TS>::DiffusionTensorFunctor::OrderingType {
        bool operator()(int i, int j) const {return this->e[i]<this->e[j];}
        const EigenValuesArrayType & e;
        OrderingType(const EigenValuesArrayType & e_):e(e_){};
    };
    
    template<typename TI, typename TS>
    void AnisotropicDiffusionLBRImageFilter<TI,TS>::ComputeDiffusionTensors(ImageType*image){
        typename StructureTensorFilterType::Pointer structureTensorFilter = StructureTensorFilterType::New();
        
        structureTensorFilter->SetNoiseScale(m_NoiseScale);
        structureTensorFilter->SetFeatureScale(m_FeatureScale);
        structureTensorFilter->SetRescaleForUnitMaximumTrace(m_Adimensionize);
        structureTensorFilter->SetInput(image);
        
        typedef UnaryFunctorImageFilter<TensorImageType, TensorImageType, DiffusionTensorFunctor> ImageFunctorType;
        typename ImageFunctorType::Pointer imageFunctor = ImageFunctorType::New();
        imageFunctor->GetFunctor().eigenValuesFunctor = this;
        imageFunctor->SetInput(structureTensorFilter->GetOutput());
        
        imageFunctor->Update();
        tensorImage=imageFunctor->GetOutput();        
    }
    
}




#endif
